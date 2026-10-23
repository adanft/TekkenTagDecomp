#!/usr/bin/env python3
"""Runs tools/decomp-permuter with the other definitions of base.c kept, so candidates compile as in the build.

Usage: .venv/bin/python scripts/permuter_main.py tools/decomp-permuter/permuter.py <permuter arguments>
(scripts/permute.py --run starts it; the permuter's path stays on the command line, so
`pgrep -fc 'decomp-permute[r]/permuter.py'` still counts the run.)

The permuter's extract_fn turns every function definition of base.c other than the target into a prototype, so it
scores the target compiled out of its file. With this compiler the definitions before a function can change its code:
func_003EFDB8 of tekken_bowl.c gets the register of the build only after func_003EF818 is compiled. Here extract_fn
keeps them, and the workers are forked (the default start method of Python 3.14 is forkserver, which would re-import
the permuter without the patch in each worker). The vendored permuter is not changed.
"""

import importlib
import multiprocessing
import sys
from pathlib import Path

PERMUTER = Path(__file__).resolve().parent.parent / "tools" / "decomp-permuter"


def permuter_module(name):
    """A module of the permuter's package (its `src`, not the repository's src/)."""
    if str(PERMUTER) not in sys.path:
        sys.path.insert(0, str(PERMUTER))
    return importlib.import_module(name)


def extract_fn_keep_context(ast, fn_name):
    """ast_util.extract_fn without turning the other definitions into prototypes: (the definition, its index)."""
    c_ast = permuter_module("perm_pycparser.c_ast")
    failure = permuter_module("src.error").CandidateConstructionFailure
    found = [(node, i) for i, node in enumerate(ast.ext)
             if isinstance(node, c_ast.FuncDef) and node.decl.name == fn_name]
    if not found:
        raise failure(f"Function {fn_name} not found in base.c.")
    if len(found) > 1:
        raise failure(f"Found multiple copies of function {fn_name} in base.c.")
    return found[0]


def install():
    permuter_module("src.ast_util").extract_fn = extract_fn_keep_context


def main():
    if len(sys.argv) < 2 or Path(sys.argv[1]).resolve() != PERMUTER / "permuter.py":
        sys.exit(__doc__)
    multiprocessing.set_start_method("fork", force=True)
    install()
    sys.argv = [sys.argv[1], *sys.argv[2:]]
    permuter_module("src.main").main()


if __name__ == "__main__":
    main()
