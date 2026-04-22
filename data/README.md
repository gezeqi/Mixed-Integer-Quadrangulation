# Data folder

Put a triangle mesh here called `bumpy.off` (or pass your own mesh path as the
first CLI argument to either executable).

Quick ways to get the canonical libigl sample used in the SIGGRAPH 2009 paper
teaser and libigl's 505_MIQ tutorial:

    # option A — grab the one libigl ships with
    curl -L -o bumpy.off https://raw.githubusercontent.com/libigl/libigl-tutorial-data/master/bumpy.off

    # option B — use any OBJ/OFF you have; tested sizes: 5k - 40k triangles
    #           larger meshes make the mixed-integer solve noticeably slower.

Known good test meshes from libigl-tutorial-data:
  bumpy.off       (≈ 3k faces, fast, a good sanity check)
  lilium.off      (≈ 10k faces, more visible singularities)
  3holes.off      (≈ 5k faces, non-trivial topology / genus)
