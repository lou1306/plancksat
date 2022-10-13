# PlanckSat 🔬

PlanckSat is just a set of small features and quality-of-life improvements
on top of the state-of-the-art [MiniSat](http://minisat.se/) solver.

## Feature list

* PlanckSat builds on macOS (tested on a MacBook Pro with Apple Silicon).
  We support building the `plancksat` binary; building of dynamic libraries
  is currently out of the scope of this project. Linux users may still
  want to use the original Makefile, kept as `Makefile.bak`.

* PlanckSat's output strives to follow DIMACS conventions, as specified
  [here](http://www.satcompetition.org/2004/format-solvers2004.html).
  Specifically: the *solution* line starts with an `s` and contains
  `SATISFIABLE`, `UNSATISFIABLE` or `UNKNOWN` (instead of `INDETERMINATE`);
  the *values* line starts with a `v`; and all other output lines start
  with a `c` *for *comment*).

* `-model` tells PlanckSat to print the satisfying assignment (if any)
  to standard output. The default is `-no-model`.

* `-rnd-pol` lets PlanckSat randomize the polarity of a variable upon
  branching, unless the user had set a preferred modality for that variable
  (see `-try-assume` below). The default is `-no-rnd-pol`.

* `-assume "<LITERALS>"` tells PlanckSat to solve under the given
  (strong) assumptions. Essentially, every literal in `LITERALS` is added to
  the problem as a separate unit clause.
  
* `-try-assume "<LITERALS>` tells PlanckSat to try using the suggested
  polarities for some variables. However, if the formula is unsatisfiable
  under all these (weak) assumptions, the solver may drop one or more
  until a solution is found or the formula is found to be `UNSAT` even
  after *all* weak assumptions have been dropped.
  
## Examples

* Basic invocation:

  ```bash
  plancksat a.cnf
  ```
  
  Note that the default values have been chosen to keep in line with the
  behaviour of vanilla MiniSat. In other words, invoking `minisat` or `plancksat`
  with the same command-line arguments should lead to the same results (modulo
  the DIMACS-compliant output).

* Strong and weak assumptions:
  
  ```bash
  plancksat a.cnf -assume "1 2 -3" -try-assume "4 -5"
  ```
  
  This tells PlanckSat to solve `a.cnf` under the assumption that `1` and `2`
  be `true`, and `3` be `false`. In other words, we are really solving
  the formula `<a.cnf> /\ (1) /\ (2) /\ (-3)`. Additionally we would
  *prefer* the solution (if any) to have an asserted `4` and a negated `5`.

## Building

To build a `plancksat` binary, execute the following commands:

```bash
make config prefix=$PREFIX
make 
```
The binary will be placed at `$PREFIX/release/bin/plancksat`

Linux users may prefer using the original Makefile and follow the original
MiniSat `README`.

