#pragma once

#include "Graphics/Post/PostEffect.h"


class DepthOfField : public PostEffect
{
public:
	//Initializes framebuffer
	//Overrides post effect Init
	void Init(unsigned width, unsigned height) override;

	//Applies the effect to this buffer
	//passes the previous framebuffer with the texture to apply as parameter
	void ApplyEffect(PostEffect* buffer) override;

	void Reshape(unsigned width, unsigned height) override;

	//Getters
	float GetRadius() const;
	float GetDownscale() const;
	unsigned GetPasses() const;
	float GetZNear() const;
	float GetZFar() const;
	float GetAperture() const;
	float GetPlaneInFocus() const;
	float GetFocalLength() const;
	float GetAmount();
	std::vector<Shader::sptr> GetShaders() const;

	//Setters
	void SetRadius(float radius);
	void SetDownscale(float downscale);
	void SetPasses(unsigned passes);
	void SetZNear(float dist);
	void SetZFar(float dist);
	void SetAperture(float aperture);
	void SetPlaneInFocus(float plane);
	void SetFocalLength(float length);
	void SetAmount(float amount);
private:
	float _downscale = 2.0f;
	unsigned _passes = 10;
	glm::vec2 _pixelSize;

	float _zNear = 0.25f;
	float _zFar = 5.0f;

	float _planeInFocus = 0.5f;

	float _focalLength = 0.2f;

	float _aperture = 2.5f;

	float _amount = 1.0f;

	float _radius;

	glm::vec3 focalPoint = glm::vec3(0.0f, 0.0f, 2.0f);
};

