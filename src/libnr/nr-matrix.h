#ifndef __NR_MATRIX_H__
#define __NR_MATRIX_H__

/*
 * Pixel buffer rendering library
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * This code is in public domain
 */

#include <libnr/nr-macros.h>
#include <libnr/nr-values.h>

#define nr_matrix_set_identity(m) (*(m) = NR_MATRIX_IDENTITY)

#define nr_matrix_test_identity(m,e) (!(m) || NR_MATRIX_DF_TEST_CLOSE (m, &NR_MATRIX_IDENTITY, e))

#define nr_matrix_test_equal(m0,m1,e) ((!(m0) && !(m1)) || ((m0) && (m1) && NR_MATRIX_DF_TEST_CLOSE (m0, m1, e)))
#define nr_matrix_test_transform_equal(m0,m1,e) ((!(m0) && !(m1)) || ((m0) && (m1) && NR_MATRIX_DF_TEST_TRANSFORM_CLOSE (m0, m1, e)))
#define nr_matrix_test_translate_equal(m0,m1,e) ((!(m0) && !(m1)) || ((m0) && (m1) && NR_MATRIX_DF_TEST_TRANSLATE_CLOSE (m0, m1, e)))

NRMatrix *nr_matrix_invert (NRMatrix *d, const NRMatrix *m);

/* d,m0,m1 needn't be distinct in any of these multiply routines. */

NRMatrix *nr_matrix_multiply (NRMatrix *d, const NRMatrix *m0, const NRMatrix *m1);

NRMatrix *nr_matrix_set_translate (NRMatrix *m, const NR::Coord x, const NR::Coord y);

NRMatrix *nr_matrix_set_scale (NRMatrix *m, const NR::Coord sx, const NR::Coord sy);

NRMatrix *nr_matrix_set_rotate (NRMatrix *m, const NR::Coord theta);

#define NR_MATRIX_DF_TRANSFORM_X(m,x,y) ((m)->c[0] * (x) + (m)->c[2] * (y) + (m)->c[4])
#define NR_MATRIX_DF_TRANSFORM_Y(m,x,y) ((m)->c[1] * (x) + (m)->c[3] * (y) + (m)->c[5])

#define NR_MATRIX_DF_EXPANSION2(m) (fabs ((m)->c[0] * (m)->c[3] - (m)->c[1] * (m)->c[2]))
#define NR_MATRIX_DF_EXPANSION(m) (sqrt (NR_MATRIX_DF_EXPANSION2 (m)))

namespace NR {

class scale : public Point {
public:
	scale(const Point &p) : Point(p) {}
};
 
class rotate : public Point {
public:
	rotate(Coord theta);
	rotate(const Point &p) : Point(p) {}
};

class translate : public Point {
public:
	translate(const Point &p) : Point(p) {}
};

inline Point operator*(const scale s, const Point v) {
	return Point(s[X]*v[X], s[Y]*v[Y]);
}

inline Point operator *(const rotate r, const Point v) {
	return Point(r[X]* v[X] - r[Y]*v[Y], r[Y]*v[X] + r[X]*v[Y]);
}

inline Point operator *(const translate t, const Point v) {
	return t + v;
}

/*
  c[] = | 0 2 | 4 |
        | 1 3 | 5 |

              x
  Points are  y  from the point of view of a matrix.
              1
*/
class Matrix {
public:
	NR::Coord c[6];

	Matrix() {
	}

	Matrix(const Matrix &m) {
		for ( int i = 0 ; i < 6 ; i++ ) {
			c[i] = m.c[i];
		}
	}

	Matrix(const scale &sm) {
		c[0] = sm[X]; c[2] = 0;      c[4] = 0;
		c[1] = 0;     c[3] =  sm[Y]; c[5] = 0;
	}

	Matrix(const rotate &rm) {
		c[0] = rm[X]; c[2] = -rm[Y]; c[4] = 0;
		c[1] = rm[Y]; c[3] =  rm[X]; c[5] = 0;
	}
	Matrix(const translate &tm) {
		c[0] = 1; c[2] = 0; c[4] = tm[X];
		c[1] = 0; c[3] = 1; c[5] = tm[Y];
	}
	Matrix(NRMatrix const *nr);
	
	bool test_identity() const;
	Matrix inverse() const;
	
	Point operator*(const Point v) const {
		// perhaps this should be done with a loop?  It's more
		// readable this way though.
		return Point(c[0]*v[X] + c[2]*v[Y] + c[4],
			     c[1]*v[X] + c[3]*v[Y] + c[5]);
	}

	void set_identity();
	
	// What do these do?  some kind of norm?
	NR::Coord det() const;
	NR::Coord descrim2() const;
	NR::Coord descrim() const;
	
	// legacy
	void copyto(NRMatrix* nrm);
	operator NRMatrix*() const;
	operator NRMatrix() const;
};

// Matrix factories
Matrix from_basis(const Point x_basis, const Point y_basis, const Point offset=Point(0,0));

Matrix identity();
//Matrix translate(const Point p);
//Matrix scale(const Point s);
//Matrix rotate(const NR::Coord angle);

double expansion(Matrix const & m);

Matrix operator*(const Matrix a, const Matrix b);

bool transform_equalp(const Matrix m0, const Matrix m1, const NR::Coord epsilon);
bool translate_equalp(const Matrix m0, const Matrix m1, const NR::Coord epsilon);

inline Point operator*(const NRMatrix& nrm, const Point &p) {
	 return Point(NR_MATRIX_DF_TRANSFORM_X(&nrm, p[X], p[Y]),
		      NR_MATRIX_DF_TRANSFORM_Y(&nrm, p[X], p[Y]));
}

/** find the smallest rectangle that contains the transformed Rect r. */
Rect operator*(const Matrix& nrm, const Rect &r);

};

#endif
