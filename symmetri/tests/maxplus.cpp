#include "symmetri/maxplus.hpp"

#include <catch2/catch_all.hpp>
#include <iostream>

using Catch::Approx;
using namespace symmetri;
auto eps = symmetri::epsilon<double>;
auto e = symmetri::e_<double>;

TEST_CASE("Matrices", "[MatrixXd]") {
  MatrixXd A(3, 3), B(3, 3), C(3, 3), ZERO(3, 3), ADD(3, 3), MUL(3, 3),
      SCHUR(3, 3), I(3, 3);
  A << 1, 2, 3, 0, 2, 1, 9, 3, 2;
  B << 4, 2, 0, 0, 2, 1, 9, 4, 2;
  C << 0, 2, 3, 2, 1, 0, 3, 2, 0;
  ZERO = MatrixXd::Zero(3, 3);
  I = MatrixXd::Identity(3, 3);
  // results
  ADD << 4, 2, 3, 0, 2, 1, 9, 4, 2;      // element-wise max(Aij,Bij)
  MUL << 12, 7, 5, 10, 5, 3, 13, 11, 9;  // normal A*B
  SCHUR << 5, 4, 3, 0, 4, 2, 18, 7, 4;   // schur is normal A+B
  SECTION("max plus zero matrix") {
    MatrixXd ANS(3, 3);
    ANS << eps, eps, eps, eps, eps, eps, eps, eps, eps;
    CHECK(ANS.isApprox(ZERO));
  }
  SECTION("max plus identity matrix") {
    MatrixXd ANS(3, 3);
    ANS << e, eps, eps, eps, e, eps, eps, eps, e;
    CHECK(ANS.isApprox(I));
  }
  SECTION("⊕-operator") {
    SECTION("Sum") { CHECK((A + B).isApprox(ADD)); }
    SECTION("Commutativity") { CHECK((A + B).isApprox(B + A)); }
    SECTION("Associactivity") { CHECK(((A + B) + C).isApprox(A + (B + C))); }
    SECTION("Idempotent") { CHECK((A + A).isApprox(A)); }
    SECTION("A zero/neutral element 𝜀") {
      CHECK((A + ZERO).isApprox(A));
      CHECK((ZERO + A).isApprox(A));
    }
  }
  SECTION("⊗-operator") {
    MatrixXd Q1(2, 2), Q2(2, 2), ANS(2, 2), Q3(2, 3), Q4(3, 2), ANS2(2, 2);
    Q1 << e, eps, 3, 2;
    Q2 << -1, 11, 1, eps;
    ANS << -1, 11, 3, 14;
    SECTION("Product 2x2") { CHECK((Q1 * Q2).isApprox(ANS)); }
    Q3 << 1, 6, 2, 8, 3, 4;
    Q4 << 2, 5, 3, 3, 1, 6;
    ANS2 << 9, 9, 10, 13;
    SECTION("Product 2x3 times 3x2") { CHECK((Q3 * Q4).isApprox(ANS2)); }
    SECTION("Product 3x3") { CHECK((A * B).isApprox(MUL)); }
    SECTION("Associativity") { CHECK(((A * B) * C).isApprox(A * (B * C))); }
    SECTION("A neutral element 𝜀") {
      CHECK((A * I).isApprox(A));
      CHECK((I * A).isApprox(A));
    }
    SECTION("An absorbing element e (e is 0, not exp(1))") {
      CHECK((A * ZERO).isApprox(ZERO));
      CHECK((ZERO * A).isApprox(ZERO));
    }
    SECTION(
        "Multiplicative Inverse if x!=𝜀 then there exists a unique y with "
        "x ⊗ y = e") {
      // ?
    }
  }
  SECTION("Distributivity") {
    CHECK((A * (B + C)).isApprox((A * B) + (A * C)));
  }

  SECTION("With vectors") {
    MatrixXd v1(1, 2), Q(2, 2), ANS1(1, 2), v2(2, 1), ANS2(2, 1);
    v1 << 2, 8;
    v2 << 2, 8;
    Q << 2, 0, eps, 5;
    ANS1 << 4, 13;
    ANS2 << 8, 13;
    CHECK((v1 * Q).isApprox(ANS1));
    CHECK((Q * v2).isApprox(ANS2));
  }
  SECTION("Scalar addition of matrix") {
    double d = 4;
    MatrixXd V(2, 1), ANS(2, 1);
    V << 2, 8;
    ANS << 4, 8;
    CHECK((d + V).isApprox(ANS));
    CHECK((V + d).isApprox(ANS));
  }
  SECTION("Scalar multiplication of matrix") {
    double d = 4;
    MatrixXd V(2, 1), ANS(2, 1);
    V << 2, 8;
    ANS << 6, 12;
    CHECK((d * V).isApprox(ANS));
    CHECK((V * d).isApprox(ANS));
  }
  SECTION("Taking powers") {
    MatrixXd A(3, 3), A2(3, 3), A3(3, 3), A4(3, 3);
    A << 0, eps, 2, 2, 0, 4, 1, 2, 3;
    A2 << 3, 4, 5, 5, 6, 7, 4, 5, 6;
    A3 << 6, 7, 8, 8, 9, 10, 7, 8, 9;
    A4 << 9, 10, 11, 11, 12, 13, 10, 11, 12;

    CHECK((A ^ 0).isApprox(I));
    CHECK((A ^ 1).isApprox(A));
    CHECK((A ^ 2).isApprox(A2));
    CHECK((A ^ 3).isApprox(A3));
    CHECK((A ^ 4).isApprox(A4));
  }
  SECTION("Spectral stuff") {
    MatrixXd A(3, 3), B(2, 2), V(3, 1);
    A << 0, eps, 2, 2, 0, 4, 1, 2, 3;
    CHECK(isIrreducible(A));
    B << 1, eps, eps, 2;
    CHECK(!isIrreducible(B));

    V << 3, 5, 4;
    double lambda = 3;
    CHECK((A * V).isApprox(lambda * V));
  }
  SECTION("Spectral stuff v2") {
    SECTION("ON LARGE SCALE MAX-PLUS ALGEBRA MODELS IN RAILWAY SYSTEMS") {
      MatrixXd A(10, 10), x0(10, 1), v(10, 1), v_est(10, 1);
      A.setConstant(eps);
      A(0, 3) = 58;
      A(1, 4) = 61;
      A(1, 8) = 36;
      A(2, 1) = 81;
      A(2, 8) = 69;
      A(3, 5) = 86;
      A(4, 0) = 0;
      A(5, 2) = 0;
      A(6, 9) = 58;
      A(7, 5) = 86;
      A(7, 6) = 61;
      A(8, 5) = 69;
      A(8, 7) = 35;
      A(9, 8) = 36;
      x0.setConstant(0);
      v << 48.7, 14.3, 47.7, 38.1, 1, 0, 24.3, 38.3, 25.7, 14;

      double lambda = 47.7;
      auto [p, q, c] = pgc(A, x0, 100);
      double lambda_est = c / (p - q);

      v_est = eigenvector(A, x0, p, q, c);
      CHECK(lambda_est == Approx(lambda).epsilon(0.05));

      std::cout << A << std::endl;
      CHECK(v.isApprox(v_est, 0.05));
      std::cout << v.transpose() << std::endl;
      std::cout << v_est.transpose() << std::endl;
      std::cout << lambda << std::endl;
      // Proof is in the pudding
      CHECK((A * v_est).isApprox(lambda_est * v_est, 0.05));
    }
    SECTION(
        "Max-plus algebra and max-plus linear discrete event systems: An "
        "introduction") {
      MatrixXd A(3, 3), x0(3, 1), v(3, 1), v_est(3, 1), v_est2(3, 1);
      A << 0, eps, 2, 2, 0, 4, 1, 2, 3;
      v << 0, 2, 1;
      x0.setConstant(0);
      double lambda = 3;
      auto [p, q, c] = pgc(A, x0, 100);
      double lambda_est = c / (p - q);
      v_est = eigenvector(A, x0, p, q, c);
      v_est2 = eigenvector(A, x0);

      CHECK(lambda_est == Approx(lambda));
      CHECK(v_est2.isApprox(v_est));
      CHECK(v.isApprox(v_est));
      // Proof is in the pudding
      CHECK((A * v_est).isApprox(lambda_est * v_est));
    }
    SECTION("2x2 eigen problem example") {
      MatrixXd A(2, 2), x0(2, 1), v(2, 1), v_est(2, 1);
      A << 3, 7, 2, 4;
      v << 9, 6.5;
      x0.setConstant(0);
      double lambda = 4.5;
      auto [p, q, c] = pgc(A, x0, 100);
      double lambda_est = c / (p - q);
      v_est = eigenvector(A, x0, p, q, c);
      std::cout << v.transpose() << std::endl;
      std::cout << v_est.transpose() << std::endl;
      std::cout << lambda << std::endl;

      CHECK(lambda_est == Approx(lambda));
      // CHECK(v.isApprox(v_est));
      // Proof is in the pudding
      CHECK((A * v_est).isApprox(lambda_est * v_est));
    }
    SECTION("4x4 eigen problem example") {
      MatrixXd A(4, 4), x0(4, 1), v(4, 1), v_est(4, 1);
      A << eps, 3, eps, 1, 2, eps, 1, eps, 1, 2, 2, eps, eps, eps, 1, eps;
      v << 10, 9.5, 9, 7.5;
      x0 << 0, eps, eps, eps;
      double lambda = 2.5;
      auto [p, q, c] = pgc(A, x0, 100);
      double lambda_est = c / (p - q);
      v_est = eigenvector(A, x0, p, q, c);
      std::cout << v.transpose() << std::endl;
      std::cout << v_est.transpose() << std::endl;
      std::cout << lambda << std::endl;

      CHECK(lambda_est == Approx(lambda));
      // CHECK(v.isApprox(v_est));
      // Proof is in the pudding
      CHECK((A * v_est).isApprox(lambda_est * v_est));
    }
  }
}
