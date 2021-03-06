//------------------------------------------------------------------------------------------------------------------------------
#include "Game/CarController.hpp"
#include "Engine/Commons/EngineCommon.hpp"
#include "Engine/Input/XboxController.hpp"
#include "Engine/Input/InputSystem.hpp"
//PhysX
#include "ThirdParty/PhysX/include/vehicle/PxVehicleUtil.h"

//------------------------------------------------------------------------------------------------------------------------------
// GLOBAL DATA
//------------------------------------------------------------------------------------------------------------------------------
float gSteerVsForwardSpeedData[2 * 8] =
{
	//SteerAmount	to	Forward Speed
	0.0f,		1.f,
	5.0f,		0.75f,
	30.0f,		0.125f,
	120.0f,		0.1f,
};

//------------------------------------------------------------------------------------------------------------------------------
CarController::CarController()
{
	SetupVehicle();
	m_vehicleInputData = new PxVehicleDrive4WRawInputData();

	//Setup the steer to speed table
	m_SteerVsForwardSpeedTable = PxFixedSizeLookupTable<8>(gSteerVsForwardSpeedData, 4);
}

//------------------------------------------------------------------------------------------------------------------------------
CarController::~CarController()
{

}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::HandleKeyPressed(unsigned char keyCode)
{
	return;

	//ReleaseAllControls();

	switch (keyCode)
	{
		case UP_ARROW:
		case W_KEY:
		{
			AccelerateForward(1.f);
			break;
		}		
		case DOWN_ARROW:
		case S_KEY:
		{
			AccelerateReverse(1.f);
			break;
		}
		case RIGHT_ARROW:
		case D_KEY:
		{
			Steer(1.f);
			break;
		}
		case LEFT_ARROW:
		case A_KEY:
		{
			Steer(-1.f);
			break;
		}
		case SPACE_KEY:
		{
			Brake();
			break;
		}
		case LCTRL_KEY:
		{
			Handbrake();
			break;
		}
		default:
			DebuggerPrintf("\n Controller Default");
			break;
	}
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::SetupVehicle()
{
	m_vehicle4W = g_PxPhysXSystem->StartUpVehicleSDK();
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::SetDigitalControlMode(bool digitalControlEnabled)
{
	m_digitalControlEnabled = digitalControlEnabled;
}

//------------------------------------------------------------------------------------------------------------------------------
bool CarController::IsDigitalInputEnabled() const
{
	return m_digitalControlEnabled;
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::Update(float deltaTime)
{
	UpdateInputs();
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::FixedUpdate(float deltaTime)
{
	VehiclePhysicsUpdate(deltaTime);
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::UpdateInputs()
{
	//First reset all the input on the car
	ReleaseAllControls();

	//Get Xbox controller data
	XboxController playerController = g_inputSystem->GetXboxController(m_controllerID);
	AnalogJoyStick leftStick = playerController.GetLeftJoystick();
	AnalogJoyStick rightStick = playerController.GetRightJoystick();

	float rightTrigger = playerController.GetRightTrigger();
	float leftTrigger = playerController.GetLeftTrigger();

	if (leftStick.GetAngleDegrees() != 0.f)
	{
		Vec2 stickPosition = leftStick.GetPosition();
		if (stickPosition.x > 0.f)
		{
			Steer(leftStick.GetMagnitude());
		}
		else
		{
			Steer(leftStick.GetMagnitude() * -1.f);
		}
	}

	//Check acceleration forward/reverse
	if (rightTrigger > 0.1f)
	{
		AccelerateForward(rightTrigger);
	}
	else
	{
		if (leftTrigger > 0.1f)
		{
			AccelerateReverse(leftTrigger);
		}
	}

	//Check brake button
	KeyButtonState buttonAState = playerController.GetButtonState(XBOX_BUTTON_ID_A);
	if (buttonAState.IsPressed())
	{
		Brake();
	}

	//Check hand brake button
	KeyButtonState buttonBState = playerController.GetButtonState(XBOX_BUTTON_ID_B);
	if (buttonBState.IsPressed())
	{
		Handbrake();
	}

}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::VehiclePhysicsUpdate(float deltaTime)
{
	VehicleSceneQueryData* vehicleSceneQueryData = g_PxPhysXSystem->GetVehicleSceneQueryData();
	PxScene* scene = g_PxPhysXSystem->GetPhysXScene();
	PxBatchQuery* batchQuery = g_PxPhysXSystem->GetPhysXBatchQuery();
	PxVehicleDrivableSurfaceToTireFrictionPairs* tireFrictionPairs = g_PxPhysXSystem->GetVehicleTireFrictionPairs();

	PxVehicleDrive4W* vehicle4W = GetVehicle();
	PxVehicleDrive4WRawInputData* vehicleInputData = GetVehicleInputData();

	if (vehicle4W == nullptr)
	{
		return;
	}

	//Update the control inputs for the vehicle.
	if (IsDigitalInputEnabled())
	{
		PxVehicleDrive4WSmoothDigitalRawInputsAndSetAnalogInputs(m_keySmoothingData, m_SteerVsForwardSpeedTable, *vehicleInputData, deltaTime, m_isVehicleInAir, *vehicle4W);
	}
	else
	{
		PxVehicleDrive4WSmoothAnalogRawInputsAndSetAnalogInputs(m_padSmoothingData, m_SteerVsForwardSpeedTable, *vehicleInputData, deltaTime, m_isVehicleInAir, *vehicle4W);
	}

	//Raycasts.
	PxVehicleWheels* vehicleWheels[6] = { vehicle4W };
	PxRaycastQueryResult* raycastResults = vehicleSceneQueryData->getRaycastQueryResultBuffer(0);
	const PxU32 raycastResultsSize = vehicleSceneQueryData->getQueryResultBufferSize();
	PxVehicleSuspensionRaycasts(batchQuery, 1, vehicleWheels, raycastResultsSize, raycastResults);

	//Vehicle update.
	const PxVec3 grav = scene->getGravity();
	PxWheelQueryResult wheelQueryResults[PX_MAX_NB_WHEELS];
	PxVehicleWheelQueryResult vehicleQueryResults[1] = { {wheelQueryResults, vehicle4W->mWheelsSimData.getNbWheels()} };
	int numVehicles = 1;
	PxVehicleUpdates(deltaTime, grav, *tireFrictionPairs, numVehicles, vehicleWheels, vehicleQueryResults);

	//Work out if the vehicle is in the air.
	//m_isVehicleInAir = vehicle4W->getRigidDynamicActor()->isSleeping() ? false : PxVehicleIsInAir(vehicleQueryResults[0]);
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::SetControllerIDToUse(int controllerID)
{
	m_controllerID = controllerID;
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::SetVehiclePosition(const Vec3& targetPosition)
{
	PxVec3 pxPosition = g_PxPhysXSystem->VecToPxVector(targetPosition);

	PxTransform pxTransform;
	pxTransform.p = pxPosition;
	pxTransform.q = m_vehicle4W->getRigidDynamicActor()->getGlobalPose().q;
	
	m_vehicle4W->getRigidDynamicActor()->setGlobalPose(pxTransform);
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::SetVehicleTransform(const Vec3& targetPosition, const PxQuat& quaternion)
{
	PxVec3 pxPosition = g_PxPhysXSystem->VecToPxVector(targetPosition);

	PxTransform pxTransform;
	pxTransform.p = pxPosition;
	pxTransform.q = quaternion;

	m_vehicle4W->getRigidDynamicActor()->setGlobalPose(pxTransform);
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::SetVehicleTransform(const PxTransform& transform)
{
	m_vehicle4W->getRigidDynamicActor()->setGlobalPose(transform);
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::SetNewPxVehicle(PxVehicleDrive4W* vehicle)
{
	//Assumes the previous reference was freed/safely released
	m_vehicle4W = vehicle;
	m_vehicle4W->getRigidDynamicActor()->clearForce();
	m_vehicle4W->getRigidDynamicActor()->clearTorque();
}

//------------------------------------------------------------------------------------------------------------------------------
physx::PxVehicleDrive4W* CarController::GetVehicle() const
{
	return m_vehicle4W;
}

//------------------------------------------------------------------------------------------------------------------------------
physx::PxVehicleDrive4WRawInputData* CarController::GetVehicleInputData() const
{
	return m_vehicleInputData;
}

//------------------------------------------------------------------------------------------------------------------------------
Vec3 CarController::GetVehiclePosition() const
{
	PxMat44 pose = m_vehicle4W->getRigidDynamicActor()->getGlobalPose();
	PxVec3 pxPosition = pose.getPosition();

	Vec3 position = g_PxPhysXSystem->PxVectorToVec(pxPosition);
	return position;
}

//------------------------------------------------------------------------------------------------------------------------------
Vec3 CarController::GetVehicleForwardBasis() const
{
	PxMat44 pose = m_vehicle4W->getRigidDynamicActor()->getGlobalPose();
	PxVec3 pxForward = pose.getBasis(2);
	Vec3 forward = g_PxPhysXSystem->PxVectorToVec(pxForward);
	return forward;
}

//------------------------------------------------------------------------------------------------------------------------------
Vec3 CarController::GetVehicleRightBasis() const
{
	PxMat44 pose = m_vehicle4W->getRigidDynamicActor()->getGlobalPose();
	PxVec3 pxRight = pose.getBasis(1);
	Vec3 right = g_PxPhysXSystem->PxVectorToVec(pxRight);
	return right;
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::AccelerateForward(float analogAcc)
{
	//If I am going reverse, change to first gear
	if (m_vehicle4W->mDriveDynData.getCurrentGear() == PxVehicleGearsData::eREVERSE)
	{
		m_vehicle4W->mDriveDynData.forceGearChange(PxVehicleGearsData::eFIRST);
	}

	if (m_digitalControlEnabled)
	{
		m_vehicleInputData->setDigitalAccel(true);
	}
	else
	{
		m_vehicleInputData->setAnalogAccel(analogAcc);
	}
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::AccelerateReverse(float analogAcc /*= 0.f*/)
{
	//Force gear change to reverse
	m_vehicle4W->mDriveDynData.forceGearChange(PxVehicleGearsData::eREVERSE);

	if (m_digitalControlEnabled)
	{
		m_vehicleInputData->setDigitalAccel(true);
	}
	else
	{
		m_vehicleInputData->setAnalogAccel(analogAcc);
	}
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::Brake()
{
	if (m_digitalControlEnabled)
	{
		m_vehicleInputData->setDigitalBrake(true);
	}
	else
	{
		m_vehicleInputData->setAnalogBrake(1.f);
	}
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::Steer(float analogSteer /*= 0.f*/)
{
	if (m_digitalControlEnabled)
	{
		ERROR_AND_DIE("Digital controls to be setup different from analog controls");
	}
	else
	{
		m_vehicleInputData->setAnalogSteer(analogSteer);
	}
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::Handbrake()
{
	if (m_digitalControlEnabled)
	{
		m_vehicleInputData->setDigitalHandbrake(true);
	}
	else
	{
		m_vehicleInputData->setAnalogHandbrake(1.f);
	}
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::ReleaseAllControls()
{
	if (m_digitalControlEnabled)
	{
		m_vehicleInputData->setDigitalAccel(false);
		m_vehicleInputData->setDigitalSteerLeft(false);
		m_vehicleInputData->setDigitalSteerRight(false);
		m_vehicleInputData->setDigitalBrake(false);
		m_vehicleInputData->setDigitalHandbrake(false);
	}
	else
	{
		m_vehicleInputData->setAnalogAccel(0.0f);
		m_vehicleInputData->setAnalogSteer(0.0f);
		m_vehicleInputData->setAnalogBrake(0.0f);
		m_vehicleInputData->setAnalogHandbrake(0.0f);
	}
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::RemoveVehicleFromScene()
{
	PxScene* scene = g_PxPhysXSystem->GetPhysXScene();

	scene->removeActor(*m_vehicle4W->getRigidDynamicActor());
	m_vehicle4W->getRigidDynamicActor()->release();

	PX_RELEASE(m_vehicle4W);
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::ReleaseVehicle()
{
	PX_RELEASE(m_vehicle4W);
}

//------------------------------------------------------------------------------------------------------------------------------
bool CarController::IsControlReleased()
{
	return m_controlReleased;
}

//------------------------------------------------------------------------------------------------------------------------------
void CarController::SetVehicleDefaultOrientation()
{
	PxTransform pose;
	pose.p = PhysXSystem::VecToPxVector(GetVehiclePosition());
	pose.q = PxQuat(0.f, 0.f, 0.f, 1.f);

	m_vehicle4W->getRigidDynamicActor()->setGlobalPose(pose);
}

