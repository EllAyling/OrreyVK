#pragma once
#ifndef SOLIDSPHERE_H
#define SOLIDSPHERE_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <vector>

class SolidSphere
{
private:
	std::vector<float> vertices;
	std::vector<float> normals;
	std::vector<float> indices;
public:
	SolidSphere();
	SolidSphere(float radius, size_t rings, size_t sectors);
	~SolidSphere();
};
#endif
