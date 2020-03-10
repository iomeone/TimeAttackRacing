#pragma once
#include "Game/CarController.hpp"
#include "Game/CarCamera.hpp"

//------------------------------------------------------------------------------------------------------------------------------
class Car
{
public:
	Car();
	~Car();

	void						StartUp(const Vec3& startPosition, int controllerID);
	void						Update(float deltaTime);
	void						FixedUpdate(float fixedTime);

	const CarCamera&			GetCarCamera() const;
	CarCamera*					GetCarCameraEditable();
	const CarController&		GetCarController() const;
	int							GetCarIndex() const;
	PxRigidDynamic*				GetCarRigidbody() const;

	void						SetCameraColorTarget(ColorTargetView* colorTargetView);
	void						SetCameraPerspectiveProjection(float m_camFOVDegrees, float nearZ, float farZ, float aspect);
	void						UpdateCarCamera(float deltaTime);
private:
	CarController*				m_controller = nullptr;
	CarCamera*					m_camera = nullptr;

	int							m_carIndex = -1;
};