// ---------------------------------------------------------------------------
// Stage 1 of MIQ (Bommes et al. 2009): generate a smooth 4-RoSy cross field.
// We use libigl's NRosy wrapper around CoMISo's mixed-integer solver.
//
// Inputs : a triangle mesh + a handful of user-picked constraint faces.
// Output : per-face representative direction X1 (and perpendicular X2),
//          visualized as red/blue line segments at each face centroid.
// ---------------------------------------------------------------------------
#include <igl/readOBJ.h>
#include <igl/readOFF.h>
#include <igl/barycenter.h>
#include <igl/avg_edge_length.h>
#include <igl/copyleft/comiso/nrosy.h>
#include <igl/opengl/glfw/Viewer.h>

#include <Eigen/Core>
#include <iostream>

int main(int argc, char* argv[])
{
    using namespace Eigen;

    const std::string mesh_path =
        (argc > 1) ? argv[1] : "data/bumpy.off";

    MatrixXd V;
    MatrixXi F;
    if (!igl::readOFF(mesh_path, V, F) && !igl::readOBJ(mesh_path, V, F)) {
        std::cerr << "Failed to load " << mesh_path << "\n";
        return 1;
    }
    std::cout << "Loaded V=" << V.rows() << "  F=" << F.rows() << "\n";

    // --- Pick a few constraint faces (hard-coded for a first run) ---------
    // In practice you'd let the user click these in the viewer.
    VectorXi b(1);          b << 0;
    MatrixXd bc(1, 3);      bc << 1, 0, 0;   // desired direction on face 0

    const int N = 4;        // 4-RoSy == cross field
    MatrixXd X1;            // per-face representative direction
    VectorXd S;             // singularity index per vertex
    igl::copyleft::comiso::nrosy(V, F, b, bc, VectorXi(), VectorXd(),
                                 MatrixXd(), N, 0.5, X1, S);

    // Build the perpendicular field X2 from per-face normal x X1
    MatrixXd FN(F.rows(), 3);
    for (int f = 0; f < F.rows(); ++f) {
        Vector3d e1 = V.row(F(f, 1)) - V.row(F(f, 0));
        Vector3d e2 = V.row(F(f, 2)) - V.row(F(f, 0));
        FN.row(f) = e1.cross(e2).normalized();
    }
    MatrixXd X2(F.rows(), 3);
    for (int f = 0; f < F.rows(); ++f)
        X2.row(f) = Vector3d(FN.row(f)).cross(Vector3d(X1.row(f)));

    // --- Visualize --------------------------------------------------------
    MatrixXd BC;
    igl::barycenter(V, F, BC);
    const double len = igl::avg_edge_length(V, F);

    igl::opengl::glfw::Viewer viewer;
    viewer.data().set_mesh(V, F);
    viewer.data().add_edges(BC, BC + len * X1, RowVector3d(1, 0, 0));
    viewer.data().add_edges(BC, BC + len * X2, RowVector3d(0, 0, 1));
    viewer.data().show_lines = false;
    std::cout << "Singularities (nonzero S): "
              << (S.array().abs() > 1e-6).count() << "\n";
    viewer.launch();
    return 0;
}
