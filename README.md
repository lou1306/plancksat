# PlanckSAT 🔬

PlanckSAT is just a set of small features and quality-of-life improvements
on top of the state-of-the-art MiniSAT solver.

## Feature list

* PlanckSAT builds on macOS (tested on a MacBook Pro with Apple Silicon).
  We support building the `plancksat` binary; building of dynamic libraries
  is currently out of the scope of this project. Linux users may still
  want to use the original Makefile, kept as `Makefile.bak`.
* PlanckSAT's output strives to follow DIMACS conventions, as specified
  [here](http://www.satcompetition.org/2004/format-solvers2004.html).
  Specifically: the *solution* line starts with an `s` and contains
  `SATISFIABLE`, `UNSATISFIABLE` or `UNKNOWN` (instead of `INDETERMINATE`);
  the *values* line starts with a `v`; and all other output lines start
  with a `c` *for *comment*).
  
  
