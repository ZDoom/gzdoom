/*
 ** i_joystick.cpp
 **
 **---------------------------------------------------------------------------
 ** Copyright 2012-2014 Alexey Lysiuk
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 **
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 ** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 ** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 ** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 ** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 ** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 ** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **---------------------------------------------------------------------------
 **
 */

#include "m_joy.h"

#include "HID_Utilities_External.h"

#include "d_event.h"
#include "doomdef.h"
#include "templates.h"


namespace
{

FString ToFString( const CFStringRef string )
{
	if ( NULL == string )
	{
		return FString();
	}

	const CFIndex stringLength = CFStringGetLength( string );
	
	if ( 0 == stringLength )
	{
		return FString();
	}

	const size_t bufferSize = CFStringGetMaximumSizeForEncoding( stringLength, kCFStringEncodingUTF8 ) + 1;
	
	char buffer[ bufferSize ];
	memset( buffer, 0, bufferSize );
	
	CFStringGetCString( string, buffer, bufferSize, kCFStringEncodingUTF8 );
	
	return FString( buffer );
}


class IOKitJoystick : public IJoystickConfig
{
public:
	explicit IOKitJoystick( IOHIDDeviceRef device );
	virtual ~IOKitJoystick();
	
	virtual FString GetName();
	virtual float GetSensitivity();
	virtual void SetSensitivity( float scale );
	
	virtual int GetNumAxes();
	virtual float GetAxisDeadZone( int axis );
	virtual EJoyAxis GetAxisMap( int axis );
	virtual const char* GetAxisName( int axis );
	virtual float GetAxisScale( int axis );
	
	virtual void SetAxisDeadZone( int axis, float deadZone );
	virtual void SetAxisMap( int axis, EJoyAxis gameAxis );
	virtual void SetAxisScale( int axis, float scale );
	
	virtual bool IsSensitivityDefault();
	virtual bool IsAxisDeadZoneDefault( int axis );
	virtual bool IsAxisMapDefault( int axis );
	virtual bool IsAxisScaleDefault( int axis );
	
	virtual void SetDefaultConfig();
	virtual FString GetIdentifier();
	
	void AddAxes( float axes[ NUM_JOYAXIS ] ) const;
	
	void Update();
	
private:
	IOHIDDeviceRef m_device;
	
	float m_sensitivity;
	
	struct AxisInfo
	{
		char name[ 64 ];
		
		float value;
		
		float deadZone;
		float defaultDeadZone;
		float sensitivity;
		float defaultSensitivity;
		
		EJoyAxis gameAxis;
		EJoyAxis defaultGameAxis;
		
		IOHIDElementRef element;
	};
	
	TArray< AxisInfo > m_axes;
	
	TArray< IOHIDElementRef > m_buttons;
	TArray< IOHIDElementRef > m_POVs;

	
	static const float DEFAULT_DEADZONE;
	static const float DEFAULT_SENSITIVITY;
	
	
	bool ProcessAxis  ( const IOHIDValueRef value );
	bool ProcessButton( const IOHIDValueRef value );
	bool ProcessPOV   ( const IOHIDValueRef value );
	
};


const float IOKitJoystick::DEFAULT_DEADZONE    = 0.25f;
const float IOKitJoystick::DEFAULT_SENSITIVITY = 1.0f;


IOKitJoystick::IOKitJoystick( IOHIDDeviceRef device )
: m_device( device )
, m_sensitivity( DEFAULT_SENSITIVITY )
{
	IOHIDElementRef element = HIDGetFirstDeviceElement( device, kHIDElementTypeInput );
	
	while ( NULL != element )
	{
		const uint32_t usagePage = IOHIDElementGetUsagePage( element );
		
		if ( kHIDPage_GenericDesktop == usagePage )
		{
			const uint32_t usage = IOHIDElementGetUsage( element );
			
			if ( kHIDUsage_GD_Slider == usage  
				|| kHIDUsage_GD_X    == usage || kHIDUsage_GD_Y  == usage || kHIDUsage_GD_Z  == usage
				|| kHIDUsage_GD_Rx   == usage || kHIDUsage_GD_Ry == usage || kHIDUsage_GD_Rz == usage )
			{
				AxisInfo axis;
				memset( &axis, 0, sizeof( axis ) );
				
				if ( const CFStringRef name = IOHIDElementGetName( element ) )
				{
					CFStringGetCString( name, axis.name, sizeof( axis.name ) - 1, kCFStringEncodingUTF8 );
				}
				else
				{
					snprintf( axis.name, sizeof( axis.name ), "Axis %i", m_axes.Size() + 1 );
				}
				
				axis.element = element;
				
				m_axes.Push( axis );
				
				IOHIDElement_SetCalibrationMin( element, -1 );
				IOHIDElement_SetCalibrationMax( element,  1 );
				
				HIDQueueElement( m_device, element );
			}
			else if ( kHIDUsage_GD_Hatswitch == usage && m_POVs.Size() < 4 )
			{
				m_POVs.Push( element );
				
				HIDQueueElement( m_device, element );
			}			
		}
		else if ( kHIDPage_Button == usagePage )
		{
			m_buttons.Push( element );
			
			HIDQueueElement( m_device, element );
		}
		
		element = HIDGetNextDeviceElement( element, kHIDElementTypeInput );
	}

	SetDefaultConfig();
}

IOKitJoystick::~IOKitJoystick()
{
	M_SaveJoystickConfig( this );
}


FString IOKitJoystick::GetName()
{
	FString result;
	
	result += ToFString( IOHIDDevice_GetManufacturer( m_device ) );
	result += " ";
	result += ToFString( IOHIDDevice_GetProduct( m_device ) );
	
	return result;
}


float IOKitJoystick::GetSensitivity()
{
	return m_sensitivity;
}

void IOKitJoystick::SetSensitivity( float scale )
{
	m_sensitivity = scale;
}


int IOKitJoystick::GetNumAxes()
{
	return static_cast< int >( m_axes.Size() );
}

#define IS_AXIS_VALID ( static_cast< unsigned int >( axis ) < m_axes.Size() )

float IOKitJoystick::GetAxisDeadZone( int axis )
{
	return IS_AXIS_VALID ? m_axes[ axis ].deadZone : 0.0f;
}

EJoyAxis IOKitJoystick::GetAxisMap( int axis )
{
	return IS_AXIS_VALID ? m_axes[ axis ].gameAxis : JOYAXIS_None;
}

const char* IOKitJoystick::GetAxisName( int axis )
{
	return IS_AXIS_VALID ? m_axes[ axis ].name : "Invalid";
}

float IOKitJoystick::GetAxisScale( int axis )
{
	return IS_AXIS_VALID ? m_axes[ axis ].sensitivity : 0.0f;
}

void IOKitJoystick::SetAxisDeadZone( int axis, float deadZone )
{
	if ( IS_AXIS_VALID )
	{
		m_axes[ axis ].deadZone = clamp( deadZone, 0.0f, 1.0f );
	}
}

void IOKitJoystick::SetAxisMap( int axis, EJoyAxis gameAxis )
{
	if ( IS_AXIS_VALID )
	{
		m_axes[ axis ].gameAxis = ( gameAxis > JOYAXIS_None && gameAxis < NUM_JOYAXIS )
			? gameAxis
			: JOYAXIS_None;
	}
}
	
void IOKitJoystick::SetAxisScale( int axis, float scale )
{
	if ( IS_AXIS_VALID )
	{
		m_axes[ axis ].sensitivity = scale;
	}
}


bool IOKitJoystick::IsSensitivityDefault()
{
	return DEFAULT_SENSITIVITY == m_sensitivity;
}

bool IOKitJoystick::IsAxisDeadZoneDefault( int axis )
{
	return IS_AXIS_VALID
		? ( m_axes[ axis ].deadZone == m_axes[ axis ].defaultDeadZone )
		: true;
}

bool IOKitJoystick::IsAxisMapDefault( int axis )
{
	return IS_AXIS_VALID
		? ( m_axes[ axis ].gameAxis == m_axes[ axis ].defaultGameAxis )
		: true;
}

bool IOKitJoystick::IsAxisScaleDefault( int axis )
{
	return IS_AXIS_VALID
		? ( m_axes[ axis ].sensitivity == m_axes[ axis ].defaultSensitivity )
		: true;
}

#undef IS_AXIS_VALID

void IOKitJoystick::SetDefaultConfig()
{
	m_sensitivity = DEFAULT_SENSITIVITY;

	const size_t axisCount = m_axes.Size();
	
	for ( size_t i = 0; i < axisCount; ++i )
	{
		m_axes[i].deadZone    = DEFAULT_DEADZONE;
		m_axes[i].sensitivity = DEFAULT_SENSITIVITY;
		m_axes[i].gameAxis    = JOYAXIS_None;
	}
	
	// Two axes? Horizontal is yaw and vertical is forward.
	
	if ( 2 == axisCount)
	{
		m_axes[0].gameAxis = JOYAXIS_Yaw;
		m_axes[1].gameAxis = JOYAXIS_Forward;
	}
	
	// Three axes? First two are movement, third is yaw.
	
	else if ( axisCount >= 3 )
	{
		m_axes[0].gameAxis = JOYAXIS_Side;
		m_axes[1].gameAxis = JOYAXIS_Forward;
		m_axes[2].gameAxis = JOYAXIS_Yaw;
		
		// Four axes? First two are movement, last two are looking around.
		
		if ( axisCount >= 4 )
		{
			m_axes[3].gameAxis = JOYAXIS_Pitch;
//	???		m_axes[3].sensitivity = 0.75f;
			
			// Five axes? Use the fifth one for moving up and down.
			
			if ( axisCount >= 5 )
			{
				m_axes[4].gameAxis = JOYAXIS_Up;
			}
		}
	}
	
	// If there is only one axis, then we make no assumptions about how
	// the user might want to use it.
	
	// Preserve defaults for config saving.
	
	for ( size_t i = 0; i < axisCount; ++i )
	{
		m_axes[i].defaultDeadZone    = m_axes[i].deadZone;
		m_axes[i].defaultSensitivity = m_axes[i].sensitivity;
		m_axes[i].defaultGameAxis    = m_axes[i].gameAxis;
	}
}


FString IOKitJoystick::GetIdentifier()
{
	char identifier[ 32 ] = {0};
	
	snprintf( identifier, sizeof( identifier ), "VID_%04lx_PID_%04lx", 
		IOHIDDevice_GetVendorID( m_device ), IOHIDDevice_GetProductID( m_device ) );
	
	return FString( identifier );
}


void IOKitJoystick::AddAxes( float axes[ NUM_JOYAXIS ] ) const
{
	for ( size_t i = 0, count = m_axes.Size(); i < count; ++i )
	{
		const EJoyAxis axis = m_axes[i].gameAxis;
		
		if ( JOYAXIS_None == axis )
		{
			continue;
		}
		
		axes[ axis ] -= m_axes[i].value;
	}
}


void IOKitJoystick::Update()
{
	IOHIDValueRef value = NULL;
	
	while ( HIDGetEvent( m_device, &value ) && NULL != value )
	{
		ProcessAxis( value ) || ProcessButton( value ) || ProcessPOV( value );
		
		CFRelease( value );
	}
}


bool IOKitJoystick::ProcessAxis( const IOHIDValueRef value )
{
	const IOHIDElementRef element = IOHIDValueGetElement( value );
	
	if ( NULL == element )
	{
		return false;
	}

	for ( size_t i = 0, count = m_axes.Size(); i < count; ++i )
	{
		if ( element != m_axes[i].element )
		{
			continue;
		}
		
		AxisInfo& axis = m_axes[i];
		
		const double scaledValue   = IOHIDValueGetScaledValue( value, kIOHIDValueScaleTypeCalibrated );
		const double filteredValue = Joy_RemoveDeadZone( scaledValue, axis.deadZone, NULL );
		
		axis.value = static_cast< float >( filteredValue * m_sensitivity * axis.sensitivity );
		
		return true;
	}
	
	return false;
}

bool IOKitJoystick::ProcessButton( const IOHIDValueRef value )
{
	const IOHIDElementRef element = IOHIDValueGetElement( value );
	
	if ( NULL == element )
	{
		return false;
	}
	
	for ( size_t i = 0, count = m_buttons.Size(); i < count; ++i )
	{
		if ( element != m_buttons[i] )
		{
			continue;
		}
		
		const int newButton = IOHIDValueGetIntegerValue( value ) & 1;
		const int oldButton = ~newButton;
		
		Joy_GenerateButtonEvents( oldButton, newButton, 1, 
			static_cast< int >( KEY_FIRSTJOYBUTTON + i ) );
		
		return true;
	}
	
	return false;
}

bool IOKitJoystick::ProcessPOV( const IOHIDValueRef value )
{
	const IOHIDElementRef element = IOHIDValueGetElement( value );
	
	if ( NULL == element )
	{
		return false;
	}
	
	for ( size_t i = 0, count = m_POVs.Size(); i < count; ++i )
	{
		if ( element != m_POVs[i] )
		{
			continue;
		}
		
		const CFIndex direction = IOHIDValueGetIntegerValue( value );

		// Default values is for Up/North
		int oldButtons = 0;
		int newButtons = 1;
		int numButtons = 1;
		int baseButton = KEY_JOYPOV1_UP;
		
		switch ( direction )
		{
			case 0: // N
				break;

			case 1: // NE
				newButtons = 3;
				numButtons = 2;
				break;
				
			case 2: // E
				baseButton = KEY_JOYPOV1_RIGHT;
				break;

			case 3: // SE
				newButtons = 3;
				numButtons = 2;
				baseButton = KEY_JOYPOV1_RIGHT;
				break;

			case 4: // S
				baseButton = KEY_JOYPOV1_DOWN;
				break;

			case 5: // SW
				newButtons = 3;
				numButtons = 2;
				baseButton = KEY_JOYPOV1_DOWN;
				break;
				
			case 6: // W
				baseButton = KEY_JOYPOV1_LEFT;
				break;
				
			case 7: // NW
				newButtons = 9; // UP and LEFT
				numButtons = 4;
				break;
				
			default:
				// release all four directions
				oldButtons = 15;
				newButtons = 0;
				numButtons = 4;
				break;
		}
		
		Joy_GenerateButtonEvents( oldButtons, newButtons, numButtons, 
			static_cast< int >( baseButton + i * 4 ) );
	}
	
	return false;
}


// ---------------------------------------------------------------------------


class IOKitJoystickManager
{
public:
	IOKitJoystickManager();
	~IOKitJoystickManager();
	
	void GetJoysticks( TArray< IJoystickConfig* >& joysticks ) const;
	
	void AddAxes( float axes[ NUM_JOYAXIS ] ) const;
	
	// Updates axes/buttons states
	void Update();
	
	// Rebuilds device list
	void Rescan();
	
private:
	TArray< IOKitJoystick* > m_joysticks;
	
	static void OnDeviceChanged( void* context, IOReturn result, void* sender, IOHIDDeviceRef device );
	
	void ReleaseJoysticks();
	
	void EnableCallbacks();
	void DisableCallbacks();
	
};


IOKitJoystickManager::IOKitJoystickManager()
{
	Rescan();
}

IOKitJoystickManager::~IOKitJoystickManager()
{
	ReleaseJoysticks();
	DisableCallbacks();
	
	HIDReleaseDeviceList();
}


void IOKitJoystickManager::GetJoysticks( TArray< IJoystickConfig* >& joysticks ) const
{
	const size_t joystickCount = m_joysticks.Size();
	
	joysticks.Resize( joystickCount );
	
	for ( size_t i = 0; i < joystickCount; ++i )
	{
		M_LoadJoystickConfig( m_joysticks[i] );
		
		joysticks[i] = m_joysticks[i];
	}
}

void IOKitJoystickManager::AddAxes( float axes[ NUM_JOYAXIS ] ) const
{
	for ( size_t i = 0, count = m_joysticks.Size(); i < count; ++i )
	{
		m_joysticks[i]->AddAxes( axes );
	}
}


void IOKitJoystickManager::Update()
{
	for ( size_t i = 0, count = m_joysticks.Size(); i < count; ++i )
	{
		m_joysticks[i]->Update();
	}
}


void IOKitJoystickManager::Rescan()
{
	ReleaseJoysticks();
	DisableCallbacks();
	
	const int usageCount = 2;
	
	const UInt32 usagePages[ usageCount ] =
	{
		kHIDPage_GenericDesktop,
		kHIDPage_GenericDesktop
	};
	
	const UInt32 usages[ usageCount ] =
	{
		kHIDUsage_GD_Joystick,
		kHIDUsage_GD_GamePad
	};
	
	if ( HIDUpdateDeviceList( usagePages, usages, usageCount ) )
	{
		IOHIDDeviceRef device = HIDGetFirstDevice();
		
		while ( NULL != device )
		{		
			IOKitJoystick* joystick = new IOKitJoystick( device );
			m_joysticks.Push( joystick );
			
			device = HIDGetNextDevice( device );
		}
	}
	else
	{
		Printf( "IOKitJoystickManager: Failed to build gamepad/joystick device list.\n" );
	}
	
	EnableCallbacks();
}


void IOKitJoystickManager::OnDeviceChanged( void* context, IOReturn result, void* sender, IOHIDDeviceRef device )
{
	event_t event;
	
	memset( &event, 0, sizeof( event ) );
	event.type = EV_DeviceChange;
	
	D_PostEvent( &event );
}


void IOKitJoystickManager::ReleaseJoysticks()
{
	for ( size_t i = 0, count = m_joysticks.Size(); i < count; ++i )
	{
		delete m_joysticks[i];
	}
	
	m_joysticks.Clear();
}

	
void IOKitJoystickManager::EnableCallbacks()
{
	if ( NULL == gIOHIDManagerRef )
	{
		return;
	}	
	
	IOHIDManagerRegisterDeviceMatchingCallback( gIOHIDManagerRef, OnDeviceChanged, this );
	IOHIDManagerRegisterDeviceRemovalCallback ( gIOHIDManagerRef, OnDeviceChanged, this );
	IOHIDManagerScheduleWithRunLoop( gIOHIDManagerRef, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode );
}

void IOKitJoystickManager::DisableCallbacks()
{
	if ( NULL == gIOHIDManagerRef )
	{
		return;
	}	
	
	IOHIDManagerUnscheduleFromRunLoop( gIOHIDManagerRef, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode );
	IOHIDManagerRegisterDeviceMatchingCallback( gIOHIDManagerRef, NULL, NULL );
	IOHIDManagerRegisterDeviceRemovalCallback ( gIOHIDManagerRef, NULL, NULL );
}


IOKitJoystickManager* s_joystickManager;

	
} // unnamed namespace

	
// ---------------------------------------------------------------------------
	

void I_StartupJoysticks()
{
	s_joystickManager = new IOKitJoystickManager;
}

void I_ShutdownJoysticks()
{
	delete s_joystickManager;
}

void I_GetJoysticks( TArray< IJoystickConfig* >& sticks )
{
	if ( NULL != s_joystickManager )
	{
		s_joystickManager->GetJoysticks( sticks );
	}
}

void I_GetAxes( float axes[ NUM_JOYAXIS ] )
{
	for ( size_t i = 0; i < NUM_JOYAXIS; ++i )
	{
		axes[i] = 0.0f;
	}
	
	if ( use_joystick && NULL != s_joystickManager )
	{
		s_joystickManager->AddAxes( axes );
	}
}

IJoystickConfig* I_UpdateDeviceList()
{
	if ( use_joystick && NULL != s_joystickManager )
	{
		s_joystickManager->Rescan();
	}
	
	return NULL;
}


// ---------------------------------------------------------------------------


void I_ProcessJoysticks()
{
	if ( use_joystick && NULL != s_joystickManager )
	{
		s_joystickManager->Update();
	}
}
