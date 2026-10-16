"""Prepare the shared DXVK tree using Underground 2's maintained recipe.

Both Android builds consume the same prepared source and must apply the
same cleanup and pipeline-cache patches. Arguments are source and output.
"""
import pathlib
import runpy

recipe = pathlib.Path(__file__).resolve().parents[3] / 'nfsu2-recompiled/ports/dxvk_android/prepare.py'
if not recipe.is_file():
    raise SystemExit('The sibling nfsu2-recompiled checkout is required for the shared Android DXVK backend')
runpy.run_path(str(recipe), run_name='__main__')
