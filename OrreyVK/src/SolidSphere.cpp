#include "SolidSphere.h"

SolidSphere::SolidSphere()
{

}

SolidSphere::SolidSphere(float radius, size_t rings, size_t sectors)		//Create the planet spheare with normals.
{
	float const R = 1. / (float)(rings - 1.0);
	float const S = 1. / (float)(sectors - 1.0);
	int r, s;

	vertices.resize(rings * sectors * 3.0);
	normals.resize(rings * sectors * 3.0);
	std::vector<float>::iterator v = vertices.begin();
	std::vector<float>::iterator n = normals.begin();
	for (r = 0; r < rings; r++) for (s = 0; s < sectors; s++) {
		float const y = sin(-M_PI_2 + M_PI * r * R);
		float const x = cos(2 * M_PI * s * S) * sin(M_PI * r * R);
		float const z = sin(2 * M_PI * s * S) * sin(M_PI * r * R);

		*v++ = x * radius;
		*v++ = y * radius;
		*v++ = z * radius;

		*n++ = x;
		*n++ = y;
		*n++ = z;
	}

	indices.resize(rings * sectors * 4);
	std::vector<float>::iterator i = indices.begin();
	for (r = 0; r < rings - 1; r++) for (s = 0; s < sectors - 1; s++) {
		*i++ = r * sectors + s;
		*i++ = r * sectors + (s + 1.0);
		*i++ = (r + 1) * sectors + (s + 1.0);
		*i++ = (r + 1) * sectors + s;
	}
}

SolidSphere::~SolidSphere()
{

}