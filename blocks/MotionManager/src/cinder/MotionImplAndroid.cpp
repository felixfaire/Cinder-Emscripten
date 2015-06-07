/*
 Copyright (c) 2012, The Cinder Project, All rights reserved.

 This code is intended for use with the Cinder C++ library: http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#include "MotionImplAndroid.h"
#include "cinder/app/android/EventManagerAndroid.h"
#include "cinder/app/App.h"
using namespace ci::app;

#include "glm/gtx/matrix_interpolation.hpp"

namespace cinder {

MotionImplAndroid::MotionImplAndroid()
{
}

MotionImplAndroid::~MotionImplAndroid()
{
}

bool MotionImplAndroid::isMotionUpdatesActive() const
{
	return false;
}

bool MotionImplAndroid::isMotionDataAvailable() const
{
	return false;
}

bool MotionImplAndroid::isGyroAvailable() const
{
	auto eventManager = EventManagerAndroid::instance();
	return ( nullptr != eventManager ) && eventManager->isGyroscopeAvailable();
}

bool MotionImplAndroid::isAccelAvailable() const
{
	auto eventManager = EventManagerAndroid::instance();
	return ( nullptr != eventManager ) && eventManager->isAccelerometerAvailable();
}

bool MotionImplAndroid::isNorthReliable() const
{
	auto eventManager = EventManagerAndroid::instance();
	return ( nullptr != eventManager ) && eventManager->isMagneticFieldAvailable();
}

void MotionImplAndroid::startMotionUpdates()
{
console() << "MotionImplAndroid::startMotionUpdates" << std::endl;

	auto eventManager = EventManagerAndroid::instance();

	int32_t usec = (int32_t)( 1.0f / mUpdateFrequency * 1.0e6f );

	auto updateAccelerometerFn = std::bind( &MotionImplAndroid::updateAccelerometer, this, std::placeholders::_1 );
	eventManager->enableAccelerometer( updateAccelerometerFn, usec );

	auto updateMagneticFieldFn = std::bind( &MotionImplAndroid::updateMagneticField, this, std::placeholders::_1 );
	eventManager->enableMagneticField( updateMagneticFieldFn, usec );

	auto updateGyroscopeFn = std::bind( &MotionImplAndroid::updateGyroscope, this, std::placeholders::_1 );
	eventManager->enableGyroscope( updateGyroscopeFn, usec );

	mHasAccelerometer = false;
	mHasMagneticField = false;
	mHasGyroscope = false;

	/*
	if( MotionManager::Accelerometer == mSensorMode ) {
		auto updateAccelerometerFn = std::bind( &MotionImplAndroid::updateAccelerometer, this, std::placeholders::_1 );
		eventManager->enableAccelerometer( updateAccelerometerFn, usec );
	}
	else if( MotionManager::Gyroscope == mSensorMode ) {
		auto updateGyroscopeFn = std::bind( &MotionImplAndroid::updateGyroscope, this, std::placeholders::_1 );
		eventManager->enableGyroscope( updateGyroscopeFn, usec );
	}
	*/
}

void MotionImplAndroid::stopMotionUpdates()
{
	auto eventManager = EventManagerAndroid::instance();

	eventManager->disableAccelerometer();
	eventManager->disableMagneticField();
	eventManager->disableGyroscope();


	/*
	if( MotionManager::Accelerometer == mSensorMode ) {
		eventManager->disableAccelerometer();
	}
	else if( MotionManager::Gyroscope == mSensorMode ) {
		eventManager->disableGyroscope();
	}
	*/
}

void MotionImplAndroid::setSensorMode( MotionManager::SensorMode mode )
{
	if( mode == mSensorMode ) {
		return;
	}

	mSensorMode = mode;
}

void MotionImplAndroid::setUpdateFrequency( float updateFrequency )
{
	mUpdateFrequency = updateFrequency;
}

void MotionImplAndroid::setShowsCalibrationView( bool shouldShow )
{

}

ci::vec3 MotionImplAndroid::getGravityDirection( app::InterfaceOrientation orientation ) const
{
	return ci::vec3();
}

ci::quat MotionImplAndroid::getRotation( app::InterfaceOrientation orientation ) const
{
	return toQuat( getRotationMatrix( orientation ) );
}

ci::mat4 MotionImplAndroid::getRotationMatrix( app::InterfaceOrientation orientation ) const
{
	ci::mat4 result;

	if( mHasAccelerometer && mHasMagneticField ) {
		const float* gravity = reinterpret_cast<const float*>( &mAccelerometer );
		const float* geomagnetic = reinterpret_cast<const float*>( &mMagneticField );

        float Ax = gravity[0];
        float Ay = gravity[1];
        float Az = gravity[2];
        float Ex = geomagnetic[0];
        float Ey = geomagnetic[1];
        float Ez = geomagnetic[2];
        float Hx = Ey*Az - Ez*Ay;
        float Hy = Ez*Ax - Ex*Az;
        float Hz = Ex*Ay - Ey*Ax;
        float normH = sqrtf( Hx*Hx + Hy*Hy + Hz*Hz );

        // Don't calculate if device is close to free fall (or in space?), or close to
        // magnetic north pole. Typical values are  > 100.
        if (normH >= 0.1f) {
	        float invH = 1.0f / normH;
	        Hx *= invH;
	        Hy *= invH;
	        Hz *= invH;
	        float invA = 1.0f / sqrtf(Ax*Ax + Ay*Ay + Az*Az);
	        Ax *= invA;
	        Ay *= invA;
	        Az *= invA;
	       	float Mx = Ay*Hz - Az*Hy;
	        float My = Az*Hx - Ax*Hz;
	       	float Mz = Ax*Hy - Ay*Hx;

	       	float* R = reinterpret_cast<float*>( &result );
	        R[ 0] = Hx;   R[ 1] = Hy;   R[ 2] = Hz;   R[3]  = 0.0f;
	        R[ 4] = Mx;   R[ 5] = My;   R[ 6] = Mz;   R[7]  = 0.0f;
	        R[ 8] = Ax;   R[ 9] = Ay;   R[10] = Az;   R[11] = 0.0f;
	        R[12] = 0.0f; R[13] = 0.0f; R[14] = 0.0f; R[15] = 1.0f;
	    }
    }

	static const float kPiOverTwo = M_PI / 2.0f;

	ci::mat4 correctionMatrix;

	switch( orientation ) {
		case app::PortraitUpsideDown: {
			correctionMatrix = glm::axisAngleMatrix( vec3( 0, 0, 1 ), (float)M_PI );
		}		
		break;

		case app::LandscapeLeft: {
			correctionMatrix = glm::axisAngleMatrix( vec3( 0, 0, 1 ), (float)kPiOverTwo );
		}
		break;

		case app::LandscapeRight: {
			correctionMatrix = glm::axisAngleMatrix( vec3( 0, 0, -1 ), (float)kPiOverTwo );
		}
	}

	result = correctionMatrix*result;

	return result;
}

ci::vec3 MotionImplAndroid::getRotationRate( app::InterfaceOrientation orientation ) const
{
	return ci::vec3();
}

ci::vec3 MotionImplAndroid::getAcceleration( app::InterfaceOrientation orientation ) const
{
	return ci::vec3();
}

void MotionImplAndroid::updateAccelerometer( const ci::vec3& data )
{
//console() << "MotionImplAndroid::updateAccelerometer" << std::endl;

	if( mHasAccelerometer ) {
		mAccelerometer += mAccelFilter*(data - mAccelerometer);
	}
	else {
		mAccelerometer = data;
		mHasAccelerometer = true;
	}
}

void MotionImplAndroid::updateMagneticField( const ci::vec3& data )
{
//console() << "MotionImplAndroid::updateMagneticField" << std::endl;

	if( mHasMagneticField ) {
		mMagneticField += mAccelFilter*(data - mMagneticField);
	}
	else {
		mMagneticField = data;
		mHasMagneticField = true;
	}
}

void MotionImplAndroid::updateGyroscope( const ci::vec3& data )
{
	if( mHasGyroscope ) {
		mGyroscope += mAccelFilter*(data - mGyroscope);
	}
	else {
		mGyroscope = data;
		mHasGyroscope = true;
	}
}

} // namespace cinder