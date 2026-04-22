// ---------------------------------------------------------------------------
// Stage 2 of MIQ: seamless (u,v) parametrization whose gradient aligns with
// the cross field, with integer translation + 90-deg rotation jumps across
// seams. Solved by CoMISo's greedy mixed-integer rounder.
//
// After solving we visualize:
//   * the original surface colored by iso-lines of u and v;
//   * integer iso-lines overlayed in white --- these are the quad edges.
// ---------------------------------------------------------------------------
#include <igl/readOBJ.h>
#include <igl/readOFF.h>
#include <igl/barycenter.h>
#include <igl/avg_edge_length.h>
#include <igl/per_face_normals.h>
#include <igl/local_basis.h>
#include <igl/copyleft/comiso/nrosy.h>
#include <igl/copyleft/comiso/miq.h>
#include <igl/opengl/glfw/Viewer.h>
#include <igl/PI.h>

#include <Eigen/Core>
#include <iostream>

int main(int argc, char* argv[])
{
    using namespace Eigen;
    std::cout << "[trace] enter main" << std::endl;

    const std::string mesh_path =
        (argc > 1) ? argv[1] : "data/bumpy.off";

    MatrixXd V; MatrixXi F;
    if (!igl::readOFF(mesh_path, V, F) && !igl::readOBJ(mesh_path, V, F)) {
        std::cerr << "Failed to load " << mesh_path << std::endl; return 1;
    }
    std::cout << "[trace] loaded V=" << V.rows() << " F=" << F.rows() << std::endl;

    // ---------- 1) Cross field (same as Stage 1) -------------------------
    VectorXi b(1); b << 0;
    MatrixXd bc(1, 3); bc << 1, 0, 0;
    const int N = 4;
    MatrixXd X1; VectorXd S;
    std::cout << "[trace] calling nrosy..." << std::endl;
    igl::copyleft::comiso::nrosy(V, F, b, bc, VectorXi(), VectorXd(),
                                 MatrixXd(), N, 0.5, X1, S);
    std::cout << "[trace] nrosy done; X1 rows=" << X1.rows() << std::endl;

    // Build perpendicular X2 = n x X1 (MIQ wants BOTH directions of the cross)
    MatrixXd B1, B2, B3;
    igl::local_basis(V, F, B1, B2, B3);   // B3 == face normal
    MatrixXd X2(F.rows(), 3);
    for (int f = 0; f < F.rows(); ++f)
        X2.row(f) = Vector3d(B3.row(f)).cross(Vector3d(X1.row(f)));
    std::cout << "[trace] X2 built" << std::endl;

    // ---------- 2) Mixed-integer seamless parametrization ----------------
    const double gradient_size  = 50.0;
    const double stiffness      = 5.0;
    const bool   direct_round   = false;
    const unsigned int iter     = 0;
    const unsigned int local_iter = 5;
    const bool   do_round        = true;
    const bool   singular_round  = true;
    const std::vector<int> round_v;
    const std::vector<std::vector<int>> hard_features;

    MatrixXd UV;
    MatrixXi FUV;
    std::cout << "[trace] calling miq..." << std::endl;
    igl::copyleft::comiso::miq(
        V, F, X1, X2,
        UV, FUV,
        gradient_size, stiffness,
        direct_round, iter, local_iter,
        do_round, singular_round,
        round_v, hard_features);
    std::cout << "[trace] miq done; UV=" << UV.rows()
              << " FUV=" << FUV.rows() << std::endl;

    // ---------- 3) Visualize UV + iso-lines ------------------------------
    igl::opengl::glfw::Viewer viewer;
    viewer.data().set_mesh(V, F);
    viewer.data().set_uv(UV, FUV);
    viewer.data().show_texture = true;
    viewer.data().show_lines   = false;

    std::cout << "[trace] launching viewer" << std::endl;
    viewer.launch();
    return 0;
}
