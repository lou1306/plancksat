/*****************************************************************************************[Main.cc]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007,      Niklas Sorensson
Hacks and slashes (c) 2022 Luca Di Stefano based on previous work by Omar Inverso

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include <errno.h>

#include <signal.h>
#include <zlib.h>
#include <sys/resource.h>

#include "minisat/utils/System.h"
#include "minisat/utils/ParseUtils.h"
#include "minisat/utils/Options.h"
#include "minisat/core/Dimacs.h"
#include "minisat/simp/SimpSolver.h"

using namespace Minisat;

//=================================================================================================


void printStats(Solver& solver)
{
    double cpu_time = cpuTime();
    double mem_used = memUsedPeak();
    printf("c restarts              : %" PRIu64"\n", solver.starts);
    printf("c conflicts             : %-12" PRIu64"   (%.0f /sec)\n", solver.conflicts   , solver.conflicts   /cpu_time);
    printf("c decisions             : %-12" PRIu64"   (%4.2f %% random) (%.0f /sec)\n", solver.decisions, (float)solver.rnd_decisions*100 / (float)solver.decisions, solver.decisions   /cpu_time);
    printf("c propagations          : %-12" PRIu64"   (%.0f /sec)\n", solver.propagations, solver.propagations/cpu_time);
    printf("c conflict literals     : %-12" PRIu64"   (%4.2f %% deleted)\n", solver.tot_literals, (solver.max_literals - solver.tot_literals)*100 / (double)solver.max_literals);
    if (mem_used != 0) printf("c Memory used           : %.2f MB\n", mem_used);
    printf("c CPU time              : %g s\n", cpu_time);
}


static Solver* solver;
// Terminate by notifying the solver and back out gracefully. This is mainly to have a test-case
// for this feature of the Solver as it may take longer than an immediate call to '_exit()'.
static void SIGINT_interrupt(int) { solver->interrupt(); }

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).
static void SIGINT_exit(int) {
    printf("\n"); printf("c *** INTERRUPTED ***\n");
    if (solver->verbosity > 0){
        printStats(*solver);
        printf("\n"); printf("c *** INTERRUPTED ***\n"); }
    _exit(1); }


//=================================================================================================
// Main:

int main(int argc, char** argv)
{
    try {
        setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n");
        printf("c This is PlanckSAT 1.2\n");
        printf("c Based on MiniSAT 2.2.1\n");
        
#if defined(__linux__) && defined(_FPU_EXTENDED) && defined(_FPU_DOUBLE) && defined(_FPU_GETCW)
        fpu_control_t oldcw, newcw;
        _FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
        printf("c WARNING: for repeatability, setting FPU to use double precision\n");
#endif
        // Extra options:
        //
        IntOption    verb   ("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 1, IntRange(0, 2));
        BoolOption   pre    ("MAIN", "pre",    "Completely turn on/off any preprocessing.", true);
        BoolOption   mdl    ("MAIN", "model",    "Print model to stdout.", false);
        StringOption dimacs ("MAIN", "dimacs", "If given, stop after preprocessing and write the result to this file.");
        IntOption    cpu_lim("MAIN", "cpu-lim","Limit on CPU time allowed in seconds.\n", INT32_MAX, IntRange(0, INT32_MAX));
        IntOption    mem_lim("MAIN", "mem-lim","Limit on memory usage in megabytes.\n", INT32_MAX, IntRange(0, INT32_MAX));

        // LUCA 20221003
        StringOption assume("MAIN", "assume", "Solve under these additional clauses.", "");
        StringOption try_assume("MAIN", "try-assume", "Try solving under this additional clause, but feel free to drop any of its literals", "");
        StringOption try_assume_from("MAIN", "try-assume-from", "Read the try-assume clause from file", "");

        parseOptions(argc, argv, true);
        
        SimpSolver  S;
        double      initial_time = cpuTime();

        if (!pre) S.eliminate(true);

        S.verbosity = verb;
        // printf("c Solver settings: -rnd-seed %f, -rnd-freq %f", S.random_seed, S.random_var_freq);
        
        solver = &S;
        // Use signal handlers that forcibly quit until the solver will be able to respond to
        // interrupts:
        signal(SIGINT, SIGINT_exit);
        signal(SIGXCPU,SIGINT_exit);

        // Set limit on CPU-time:
        if (cpu_lim != INT32_MAX){
            rlimit rl;
            getrlimit(RLIMIT_CPU, &rl);
            if (rl.rlim_max == RLIM_INFINITY || (rlim_t)cpu_lim < rl.rlim_max){
                rl.rlim_cur = cpu_lim;
                if (setrlimit(RLIMIT_CPU, &rl) == -1)
                    printf("c WARNING! Could not set resource limit: CPU-time.\n");
            } }

        // Set limit on virtual memory:
        if (mem_lim != INT32_MAX){
            rlim_t new_mem_lim = (rlim_t)mem_lim * 1024*1024;
            rlimit rl;
            getrlimit(RLIMIT_AS, &rl);
            if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max){
                rl.rlim_cur = new_mem_lim;
                if (setrlimit(RLIMIT_AS, &rl) == -1)
                    printf("c WARNING! Could not set resource limit: Virtual memory.\n");
            } }
        
        if (argc == 1)
            printf("c Reading from standard input... Use '--help' for help.\n");

        gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
        if (in == NULL)
            printf("c ERROR! Could not open file: %s\n", argc == 1 ? "<stdin>" : argv[1]), exit(1);
        
        if (S.verbosity > 0){
            printf("c ============================[ Problem Statistics ]=============================\n");
            printf("c |                                                                             |\n"); }
        
        parse_DIMACS(in, S);
        gzclose(in);
        FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;

        if (S.verbosity > 0){
            printf("c |  Number of variables:  %12d                                         |\n", S.nVars());
            printf("c |  Number of clauses:    %12d                                         |\n", S.nClauses()); }
        
        double parsed_time = cpuTime();
        if (S.verbosity > 0)
            printf("c |  Parse time:           %12.2f s                                       |\n", parsed_time - initial_time);

        // Change to signal-handlers that will only notify the solver and allow it to terminate
        // voluntarily:
        signal(SIGINT, SIGINT_interrupt);
        signal(SIGXCPU,SIGINT_interrupt);

        S.eliminate(true);
        double simplified_time = cpuTime();
        if (S.verbosity > 0){
            printf("c |  Simplification time:  %12.2f s                                       |\n", simplified_time - parsed_time);
        }

        if (!S.okay()){
            if (res != NULL) fprintf(res, "UNSAT\n"), fclose(res);
            if (S.verbosity > 0){
                printf("c ===============================================================================\n");
                printf("c Solved by simplification\n");
                printStats(S);
                printf("\n"); }
            printf("s UNSATISFIABLE\n");
            exit(20);
        }

        if (dimacs){
            if (S.verbosity > 0)
                printf("c ==============================[ Writing DIMACS ]===============================\n");
            S.toDimacs((const char*)dimacs);
            if (S.verbosity > 0)
                printStats(S);
            exit(0);
        }

        vec<Lit> dummy;
        unsigned int countWeakAssumes = 0;
        //=====================================================================
        // LUCA 20221003
        // This is where we inject our assumptions.
        // 1. "assume"s are unit clauses that we just push onto the solver.
        // vec<Lit> nextUnit;

        // First, we have to do this little dance to appease readClause()
        char* str;
        str = strdup((const char*)assume);
        strcat(str, " 0");
        
        // Read the string into a vector of literals.
        // These will be passed to minisat as assumptions
        readClause(str, S, dummy);
        
        // 2. For "try-assumes" we set polarities according to the user's wishes
        // (But the solver may still change them if needed)
        const char* str1 = try_assume;
        int v = 0;
        lbool x;
        while (*str1 != '\0') {
            v = parseInt(str1);
            S.pushWeak(v);
            countWeakAssumes++;
        }

        const char* strFilename = try_assume_from;
        if (strcmp(strFilename, "")) {
            gzFile inWeakAssumes = gzopen(strFilename, "rb");
            if (inWeakAssumes == NULL)
                printf("c ERROR! Could not open file: %s\n", strFilename), exit(1);
            StreamBuffer inStream(inWeakAssumes);
            int v = 0;
            lbool x;
            while (!isEof(inStream)) {
                v = parseInt(inStream);
                S.pushWeak(v);
                skipWhitespace(inStream);
                countWeakAssumes++;
            }
            gzclose(inWeakAssumes);
        }
        if (S.verbosity > 0) {
            printf("c |  Weak assumptions:  %12d                                            |\n", countWeakAssumes);
            printf("c |                                                                             |\n");
        }
        
        //=====================================================================

        lbool ret = S.solveLimited(dummy);
        
        if (S.verbosity > 0){
            printStats(S);
            printf("\n"); }
        printf(ret == l_True ? "s SATISFIABLE\n" : ret == l_False ? "s UNSATISFIABLE\n" : "s UNKNOWN\n");

        if (mdl && ret == l_True) {
            printf("\nv ");
            for (int i = 0; i < S.nVars(); i++)
                if (S.model[i] != l_Undef)
                    printf("%s%s%d", (i==0)?"":" ", (S.model[i]==l_True)?"":"-", i+1);
            printf(" 0\n");
        }

        if (res != NULL){
            if (ret == l_True){
                fprintf(res, "s SAT\nv ");
                for (int i = 0; i < S.nVars(); i++)
                    if (S.model[i] != l_Undef)
                        fprintf(res, "%s%s%d", (i==0)?"":" ", (S.model[i]==l_True)?"":"-", i+1);
                fprintf(res, " 0\n");
            }else if (ret == l_False)
                fprintf(res, "UNSAT\n");
            else
                fprintf(res, "INDET\n");
            fclose(res);
        }

#ifdef NDEBUG
        exit(ret == l_True ? 10 : ret == l_False ? 20 : 0);     // (faster than "return", which will invoke the destructor for 'Solver')
#else
        return (ret == l_True ? 10 : ret == l_False ? 20 : 0);
#endif
    } catch (OutOfMemoryException&){
        printf("c ===============================================================================\n");
        printf("s UNKNOWN\n");
        exit(0);
    }
}
