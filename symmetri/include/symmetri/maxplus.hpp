#pragma once

#include <Eigen/Dense>
#include <limits>
#include <map>
// a ⊕ b = max(a,b)
// a ⊗ b = a + b
// 𝜀 ⊗ a =
// a^(⊗n) = a ⊗ a ⊗ a ⊗ a .. n-times

namespace symmetri {

template <typename T>
static constexpr auto epsilon = std::numeric_limits<T>::lowest();
template <typename T>
static constexpr auto e_ = T(0);

template <typename Scalar>
struct maxPlusSchur {
  EIGEN_EMPTY_STRUCT_CTOR(maxPlusSchur)

  //   maxPlusSchur() {}
  const Scalar operator()(const Scalar &x, const Scalar &y) const {
    return x + y;
  }
};

template <typename Scalar>
struct maxPlusAddition {
  //   maxPlusAddition() {}
  EIGEN_EMPTY_STRUCT_CTOR(maxPlusAddition)

  const Scalar operator()(const Scalar &x, const Scalar &y) const {
    return std::max(x, y);
  }
};

class MatrixXd : public Eigen::MatrixXd {
 public:
  MatrixXd(void) : Eigen::MatrixXd() {}
  MatrixXd(size_t r, size_t c) : Eigen::MatrixXd(r, c) {}

  // This constructor allows you to construct MyVectorType from Eigen
  // expressions
  template <typename OtherDerived>
  MatrixXd(const Eigen::MatrixBase<OtherDerived> &other)
      : Eigen::MatrixXd(other) {}

  // This method allows you to assign Eigen expressions to MyVectorType
  template <typename OtherDerived>
  MatrixXd &operator=(const Eigen::MatrixBase<OtherDerived> &other) {
    this->Eigen::MatrixXd::operator=(other);
    return *this;
  }

  // Max-addition
  template <typename OtherDerived>
  MatrixXd operator+(const Eigen::MatrixBase<OtherDerived> &other) const {
    // @todo, look at packetOp vectorization stuff
    return (*this).binaryExpr(other, maxPlusAddition<Scalar>());
  }

  // Max-multiplication
  template <typename OtherDerived>
  MatrixXd operator*(const Eigen::MatrixBase<OtherDerived> &other) const {
    // assert other.rows() == this->cols()
    auto rows = this->rows();
    auto cols = other.cols();
    MatrixXd res(rows, cols);

    for (long int i = 0; i < rows; i++) {
      for (long int k = 0; k < cols; k++) {
        auto mpprod = [&]() {
          auto coeff = epsilon<double>;
          for (long int c = 0; c < this->cols(); c++) {
            coeff = std::max(coeff, this->row(i)(c) + other.col(k)(c));
          }
          return coeff;
        };
        res(i, k) = mpprod();
      }
    }
    return res;
  }

  MatrixXd operator*(double other) const {
    // @todo, look at packetOp vectorization stuff
    return (*this).unaryExpr([other](auto a) { return a + other; });
  }

  MatrixXd operator+(double other) const {
    // @todo, look at packetOp vectorization stuff
    return (*this).unaryExpr([other](auto a) { return std::max(a, other); });
  }

  template <typename OtherDerived>
  MatrixXd schur(const Eigen::MatrixBase<OtherDerived> &other) const {
    // @todo, look at packetOp vectorization stuff
    return (*this).binaryExpr(other, maxPlusSchur<Scalar>());
  }

  MatrixXd operator^(unsigned int k) const {
    // @todo, look at packetOp vectorization stuff
    auto rows = this->rows();
    auto cols = this->cols();
    if (k == 1) {
      return *this;
    } else if (k > 1) {
      MatrixXd res(rows, cols);
      // this is ofcourse naive.
      res = *this;
      for (unsigned int i = 1; i < k; i++) {
        res = (res * (*this));
      }
      return res;
    } else {
      return MatrixXd::Identity(rows, cols);
    }
  }

  static MatrixXd Zero(size_t r, size_t c) {
    return MatrixXd(r, c).setConstant(epsilon<double>);
  }

  static MatrixXd Identity(size_t r, size_t c) {
    MatrixXd m = MatrixXd::Constant(r, c, epsilon<double>);
    m.diagonal().setConstant(e_<double>);
    return m;
  }
};

MatrixXd operator*(double other, const MatrixXd &mat) { return mat * other; }
MatrixXd operator+(double other, const MatrixXd &mat) { return mat + other; }

bool isIrreducible(const MatrixXd &mat) {
  MatrixXd res = mat;
  for (unsigned int i = 1; i < mat.rows(); i++) {
    res = (res + (mat ^ i));
  }
  return !((res.array() <= epsilon<double>).any());
}

// memoize this
template <class T>
inline size_t hashf(const T &matrix) {
  size_t seed = 0;
  for (int i = 0; i < matrix.size(); i++) {
    seed ^= std::hash<int>()(i) +
            std::hash<typename T::Scalar>()(matrix.data()[i]) + 0x9e3779b9 +
            (seed << 6) + (seed >> 2);
  }
  return seed;
}

std::map<std::tuple<size_t, size_t, size_t>, MatrixXd> cache;
MatrixXd system(const MatrixXd &A, const MatrixXd &x0, size_t k = 1) {
  auto t = std::tuple{hashf(A), hashf(x0), k};
  if (cache.find(t) != cache.end()) {
    return cache[t];
  } else {
    if (k == 1) {
      return cache[t] = (A * x0);
    } else {
      return cache[t] = system(A, A * x0, k - 1);
    }
  }
}

/**
 * @brief Power method to determine eigen value, Ported from the matlab
 * toolbox
 *
 * @param A
 * @param x0
 * @param r
 * @return std::tuple<double, double, double>
 */
std::tuple<double, double, double> pgc(const MatrixXd &A, const MatrixXd &x0,
                                       size_t r = 1000) {
  std::map<size_t, MatrixXd> stans;
  const auto s = x0.size();
  for (size_t p = 1; p < r; p++) {
    stans.find(p) != stans.end() ? stans[p] : stans[p] = system(A, x0, p);
    for (size_t q = 1; q < (p - 1); q++) {
      // use normal minus operator..
      Eigen::VectorXd diff = Eigen::Map<Eigen::VectorXd>(stans[p].data(), s) -
                             Eigen::Map<Eigen::VectorXd>(stans[q].data(), s);
      double c = diff(0);
      bool test = true;
      for (long int i = 0; i < A.rows(); i++) {
        if (std::abs(diff(i) - c) > 1e-25) {
          test = false;
        }
      }
      if (test && (p > q) && (q > 0)) {
        return std::make_tuple(p, q, c);
      }
    }
  }
  // failed
  return std::make_tuple(0, 0, 0);
}

MatrixXd eigenvector(const MatrixXd &A, const MatrixXd &x0, double p, double q,
                     double c) {
  const auto lambda = c / (p - q);
  MatrixXd eigenvec = MatrixXd::Zero(A.rows(), 1);
  for (double j = 1; j <= (p - q); j += 1) {
    auto k = p - q - j;
    eigenvec = eigenvec + ((k * lambda) * system(A, x0, q + j - 1));
  }

  return eigenvec.unaryExpr(
      [b = eigenvec.minCoeff()](auto a) { return a - b; });
}

MatrixXd eigenvector(const MatrixXd &A, const MatrixXd &x0) {
  auto [p, q, c] = pgc(A, x0, 100);
  return eigenvector(A, x0, p, q, c);
}

}  // namespace symmetri
