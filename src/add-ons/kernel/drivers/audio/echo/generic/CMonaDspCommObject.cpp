// ****************************************************************************
//
//  	CMonaDspCommObject.cpp
//
//		Implementation file for EchoGals generic driver Darla 24 DSP
//		interface class.
//
//		Copyright Echo Digital Audio Corporation (c) 1998 - 2002
//		All rights reserved
//		www.echoaudio.com
//		
//		Permission is hereby granted, free of charge, to any person obtaining a
//		copy of this software and associated documentation files (the
//		"Software"), to deal with the Software without restriction, including
//		without limitation the rights to use, copy, modify, merge, publish,
//		distribute, sublicense, and/or sell copies of the Software, and to
//		permit persons to whom the Software is furnished to do so, subject to
//		the following conditions:
//		
//		- Redistributions of source code must retain the above copyright
//		notice, this list of conditions and the following disclaimers.
//		
//		- Redistributions in binary form must reproduce the above copyright
//		notice, this list of conditions and the following disclaimers in the
//		documentation and/or other materials provided with the distribution.
//		
//		- Neither the name of Echo Digital Audio, nor the names of its
//		contributors may be used to endorse or promote products derived from
//		this Software without specific prior written permission.
//
//		THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//		EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//		MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//		IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR
//		ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
//		TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//		SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE SOFTWARE.
//
// ****************************************************************************

#include "CEchoGals.h"
#include "CMonaDspCommObject.h"

#include "MonaDSP.c"
#include "Mona361DSP.c"

#include "Mona1ASIC48.c"
#include "Mona1ASIC96.c"
#include "Mona1ASIC48_361.c"
#include "Mona1ASIC96_361.c"
#include "Mona2ASIC.c"


/****************************************************************************

	Construction and destruction

 ****************************************************************************/

//===========================================================================
//
// Constructor
//
//===========================================================================

CMonaDspCommObject::CMonaDspCommObject
(
	PDWORD		pdwRegBase,				// Virtual ptr to DSP registers
	PCOsSupport	pOsSupport
) : CGMLDspCommObject( pdwRegBase, pOsSupport )
{
	strcpy( m_szCardName, "Mona" );
	m_pdwDspRegBase = pdwRegBase;		// Virtual addr DSP's register base

	m_wNumPipesOut = 14;
	m_wNumPipesIn = 12;
	m_wNumBussesOut = 14;
	m_wNumBussesIn = 12;
	m_wFirstDigitalBusOut = 6;
	m_wFirstDigitalBusIn = 4;
	
	m_bProfessionalSpdif = FALSE;

	m_fHasVmixer = FALSE;	

	m_wNumMidiOut = 0;					// # MIDI out channels
	m_wNumMidiIn = 0;						// # MIDI in  channels
	m_pDspCommPage->dwSampleRate = SWAP( (DWORD) 44100 );
												// Need this in cse we start with ESYNC
	m_bHasASIC = TRUE;
	if ( DEVICE_ID_56361 == pOsSupport->GetDeviceId() )
		m_pwDspCodeToLoad = pwMona361DSP;
	else
	
	m_pwDspCodeToLoad = pwMonaDSP;

	m_byDigitalMode = DIGITAL_MODE_SPDIF_RCA;
}	// CMonaDspCommObject::CMonaDspCommObject( DWORD dwPhysRegBase )


//===========================================================================
//
// Destructor
//
//===========================================================================

CMonaDspCommObject::~CMonaDspCommObject()
{
}	// CMonaDspCommObject::~CMonaDspCommObject()




/****************************************************************************

	Hardware setup and config

 ****************************************************************************/

//===========================================================================
//
// Mona has an ASIC on the PCI card and another ASIC in the external box; 
// both need to be loaded.
//
//===========================================================================

BOOL CMonaDspCommObject::LoadASIC()
{
	DWORD	dwControlReg;
	PBYTE	pbAsic1;
	DWORD	dwSize;

	if ( m_bASICLoaded )
		return TRUE;

	m_pOsSupport->OsSnooze( 10000 );

	if ( DEVICE_ID_56361 == m_pOsSupport->GetDeviceId() )
	{
		pbAsic1 = pbMona1ASIC48_361;
		dwSize = sizeof( pbMona1ASIC48_361 );
	}
	else
	{
		pbAsic1 = pbMona1ASIC48;
		dwSize = sizeof( pbMona1ASIC48 );
	}
	if ( !CDspCommObject::LoadASIC( DSP_FNC_LOAD_MONA_PCI_CARD_ASIC,
											  pbAsic1,
											  dwSize ) )
		return FALSE;

	m_pbyAsic = pbAsic1;	

	m_pOsSupport->OsSnooze( 10000 );

	// Do the external one
	if ( !CDspCommObject::LoadASIC( DSP_FNC_LOAD_MONA_EXTERNAL_ASIC,
											  pbMona2ASIC,
											  sizeof( pbMona2ASIC ) ) )
		return FALSE;

	m_pOsSupport->OsSnooze( 10000 );
		
	CheckAsicStatus();

	//
	// Set up the control register if the load succeeded -
	//
	// 48 kHz, internal clock, S/PDIF RCA mode
	//	
	if ( m_bASICLoaded )
	{
		dwControlReg = GML_CONVERTER_ENABLE | GML_48KHZ;
		WriteControlReg( dwControlReg );	
	}
		
	return m_bASICLoaded;
	
}	// BOOL CMonaDspCommObject::LoadASIC()


//===========================================================================
//
// Depending on what digital mode you want, Mona needs different ASICs 
//	loaded.  This function checks the ASIC needed for the new mode and sees 
// if it matches the one already loaded.
//
//===========================================================================

BOOL CMonaDspCommObject::SwitchAsic( DWORD dwMask96 )
{
	BYTE *	pbyAsicNeeded;
	DWORD		dwAsicSize;
	
	//
	//	Check the clock detect bits to see if this is
	// a single-speed clock or a double-speed clock; load
	// a new ASIC if necessary.
	// 
	if ( DEVICE_ID_56361 == m_pOsSupport->GetDeviceId() )
	{
		pbyAsicNeeded = pbMona1ASIC48_361;
		dwAsicSize = sizeof( pbMona1ASIC48_361 );
		if ( 0 != ( dwMask96 & GetInputClockDetect() ) )
		{
			pbyAsicNeeded = pbMona1ASIC96_361;
			dwAsicSize = sizeof( pbMona1ASIC96_361 );
		}
	}
	else
	{
		pbyAsicNeeded = pbMona1ASIC48;
		dwAsicSize = sizeof( pbMona1ASIC48 );
		if ( 0 != ( dwMask96 & GetInputClockDetect() ) )
		{
			pbyAsicNeeded = pbMona1ASIC96;
			dwAsicSize = sizeof( pbMona1ASIC96 );
		}
	}

	if ( pbyAsicNeeded != m_pbyAsic )
	{
		//
		// Load the desired ASIC
		//
		if ( !CDspCommObject::LoadASIC( DSP_FNC_LOAD_MONA_PCI_CARD_ASIC,
												  pbyAsicNeeded,
												  dwAsicSize ) )
			return FALSE;

		m_pbyAsic = pbyAsicNeeded;	
	}
	
	return TRUE;

}	// BOOL CMonaDspCommObject::SwitchAsic( DWORD dwMask96 )


//===========================================================================
//
// SetInputClock
//
//===========================================================================

ECHOSTATUS CMonaDspCommObject::SetInputClock(WORD wClock)
{
	BOOL			bSetRate;
	BOOL			bWriteControlReg;
	DWORD			dwControlReg, dwSampleRate;

	ECHO_DEBUGPRINTF( ("CMonaDspCommObject::SetInputClock:\n") );

	dwControlReg = GetControlRegister();

	//
	// Mask off the clock select bits
	//
	dwControlReg &= GML_CLOCK_CLEAR_MASK;
	dwSampleRate = GetSampleRate();
	
	bSetRate = FALSE;
	bWriteControlReg = TRUE;
	switch ( wClock )
	{
		case ECHO_CLOCK_INTERNAL : 
		{
			ECHO_DEBUGPRINTF( ( "\tSet Mona clock to INTERNAL\n" ) );
	
			// If the sample rate is out of range for some reason, set it
			// to a reasonable value.  mattg
			if ( ( dwSampleRate < 8000  ) ||
			     ( dwSampleRate > 96000 ) )
			{
				dwSampleRate = 48000;
			}

			bSetRate = TRUE;
			bWriteControlReg = FALSE;

			break;
		} // CLK_CLOCKININTERNAL

		case ECHO_CLOCK_SPDIF :
		{
			if ( DIGITAL_MODE_ADAT == GetDigitalMode() )
			{
				return ECHOSTATUS_CLOCK_NOT_AVAILABLE;
			}

			if ( FALSE == SwitchAsic( GML_CLOCK_DETECT_BIT_SPDIF96 ) )
			{
				return ECHOSTATUS_CLOCK_NOT_AVAILABLE;
			}
			
			ECHO_DEBUGPRINTF( ( "\tSet Mona clock to SPDIF\n" ) );
	
			dwControlReg |= GML_SPDIF_CLOCK;

			if ( GML_CLOCK_DETECT_BIT_SPDIF96 & GetInputClockDetect() )
			{
				dwControlReg |= GML_DOUBLE_SPEED_MODE;
			}
			else
			{
				dwControlReg &= ~GML_DOUBLE_SPEED_MODE;
			}
			break;
		} // CLK_CLOCKINSPDIF

		case ECHO_CLOCK_WORD : 
		{
			ECHO_DEBUGPRINTF( ( "\tSet Mona clock to WORD\n" ) );

			if ( FALSE == SwitchAsic( GML_CLOCK_DETECT_BIT_WORD96 ) )
			{
				return ECHOSTATUS_CLOCK_NOT_AVAILABLE;
			}
		
			dwControlReg |= GML_WORD_CLOCK;
			
			if ( GML_CLOCK_DETECT_BIT_WORD96 & GetInputClockDetect() )
			{
				dwControlReg |= GML_DOUBLE_SPEED_MODE;
			}
			else
			{
				dwControlReg &= ~GML_DOUBLE_SPEED_MODE;
			}
			break;
		} // CLK_CLOCKINWORD

		case ECHO_CLOCK_ADAT :
		{
			ECHO_DEBUGPRINTF( ( "\tSet Mona clock to ADAT\n" ) );
			
			if ( DIGITAL_MODE_ADAT != GetDigitalMode() )
			{
				return ECHOSTATUS_CLOCK_NOT_AVAILABLE;
			}
			
			dwControlReg |= GML_ADAT_CLOCK;
			dwControlReg &= ~GML_DOUBLE_SPEED_MODE;
			break;
		} // CLK_CLOCKINADAT

		default :
			ECHO_DEBUGPRINTF(("Input clock 0x%x not supported for Mona\n",wClock));
			ECHO_DEBUGBREAK();
				return ECHOSTATUS_CLOCK_NOT_SUPPORTED;
	}	// switch (wClock)

	//
	// Winner! Save the new input clock.
	//
	m_wInputClock = wClock;

	//
	// Do things according to the flags
	//
	if ( bWriteControlReg )
	{
		WriteControlReg( dwControlReg );
	}

	// Set Mona sample rate to something sane if word or superword is
	// being turned off
	if ( bSetRate )
	{
		SetSampleRate( GetSampleRate() );
	}

	return ECHOSTATUS_OK;
	
}	// ECHOSTATUS CMonaDspCommObject::SetInputClock



//===========================================================================
//
// SetSampleRate
// 
// Set the audio sample rate for CMona
//
//===========================================================================

DWORD CMonaDspCommObject::SetSampleRate( DWORD dwNewSampleRate )
{
	BYTE *pbyAsicNeeded;
	DWORD	dwAsicSize, dwControlReg, dwNewClock;
	
	//
	// Only set the clock for internal mode.  If the clock is not set to
	// internal, try and re-set the input clock; this more transparently
	// handles switching between single and double-speed mode
	//
	if ( GetInputClock() != ECHO_CLOCK_INTERNAL )
	{
		ECHO_DEBUGPRINTF( ( "CMonaDspCommObject::SetSampleRate: Cannot set sample rate - "
								  "clock not set to CLK_CLOCKININTERNAL\n" ) );
		
		//
		// Save the rate anyhow
		//
		m_pDspCommPage->dwSampleRate = SWAP( dwNewSampleRate );
		
		//
		// Set the input clock to the current value
		//
		SetInputClock( m_wInputClock );
		
		return GetSampleRate();
	}
	
	//
	// Now, check to see if the required ASIC is loaded
	//
	if ( dwNewSampleRate >= 88200 )
	{
		if ( DIGITAL_MODE_ADAT == GetDigitalMode() )
			return( GetSampleRate() );

		if ( DEVICE_ID_56361 == m_pOsSupport->GetDeviceId() )
		{
			pbyAsicNeeded = pbMona1ASIC96_361;
			dwAsicSize = sizeof(pbMona1ASIC96_361);
		}
		else
		{
			pbyAsicNeeded = pbMona1ASIC96;
			dwAsicSize = sizeof(pbMona1ASIC96);
		}
	}
	else
	{
		if ( DEVICE_ID_56361 == m_pOsSupport->GetDeviceId() )
		{
			pbyAsicNeeded = pbMona1ASIC48_361;
			dwAsicSize = sizeof(pbMona1ASIC48_361);
		}
		else
		{
			pbyAsicNeeded = pbMona1ASIC48;
			dwAsicSize = sizeof(pbMona1ASIC48);
		}
	}

	if ( pbyAsicNeeded != m_pbyAsic )
	{
		//
		// Load the desired ASIC
		//
		if ( FALSE == CDspCommObject::LoadASIC
													( DSP_FNC_LOAD_MONA_PCI_CARD_ASIC,
													  pbyAsicNeeded,
													  dwAsicSize ) )
			return( GetSampleRate() );

		m_pbyAsic = pbyAsicNeeded;	
	}

	//
	// Get the new control register value
	//
	dwNewClock = 0;

	dwControlReg = GetControlRegister();
	dwControlReg &= GML_CLOCK_CLEAR_MASK;
	dwControlReg &= GML_SPDIF_RATE_CLEAR_MASK;

	switch ( dwNewSampleRate )
	{
		case 96000 :
			dwNewClock = GML_96KHZ;
			break;
		
		case 88200 :
			dwNewClock = GML_88KHZ;
			break;
		
		case 48000 : 
			dwNewClock = GML_48KHZ | GML_SPDIF_SAMPLE_RATE1;
			break;
		
		case 44100 : 
			dwNewClock = GML_44KHZ;
			//
			// Professional mode
			//
			if ( dwControlReg & GML_SPDIF_PRO_MODE )
			{
				dwNewClock |= GML_SPDIF_SAMPLE_RATE0;
			}
			break;
		
		case 32000 :
			dwNewClock = GML_32KHZ | GML_SPDIF_SAMPLE_RATE0 | GML_SPDIF_SAMPLE_RATE1;
			break;
		
		case 22050 :
			dwNewClock = GML_22KHZ;
			break;
		
		case 16000 :
			dwNewClock = GML_16KHZ;
			break;
		
		case 11025 :
			dwNewClock = GML_11KHZ;
			break;
		
		case 8000 :
			dwNewClock = GML_8KHZ;
			break;
	}

	dwControlReg |= dwNewClock;

	//
	// Send the new value to the card
	//
	if ( ECHOSTATUS_OK == WriteControlReg( dwControlReg ) )
	{
		m_pDspCommPage->dwSampleRate = SWAP( dwNewSampleRate );

		ECHO_DEBUGPRINTF( ("CMonaDspCommObject::SetSampleRate: %ld "
								 "clock %ld\n", dwNewSampleRate, dwNewClock) );
	}

	return GetSampleRate();

} // DWORD CMonaDspCommObject::SetSampleRate( DWORD dwNewSampleRate )


//===========================================================================
//
//	Set digital mode
//
//===========================================================================

ECHOSTATUS CMonaDspCommObject::SetDigitalMode
(
	BYTE	byNewMode
)
{
	DWORD		dwControlReg;

	dwControlReg = GetControlRegister();
	//
	// Clear the current digital mode
	//
	dwControlReg &= GML_DIGITAL_MODE_CLEAR_MASK;

	//
	// Tweak the control reg
	//
	switch ( byNewMode )
	{
		default :
			return ECHOSTATUS_DIGITAL_MODE_NOT_SUPPORTED;
	
		case DIGITAL_MODE_SPDIF_OPTICAL :

			dwControlReg |= GML_SPDIF_OPTICAL_MODE;

			// fall through 
		
		case DIGITAL_MODE_SPDIF_RCA :
		
			//
			//	If the input clock is set to ADAT, set the 
			// input clock to internal and the sample rate to 48 KHz
			// 
			if ( ECHO_CLOCK_ADAT == GetInputClock() )
			{
				m_pDspCommPage->dwSampleRate = SWAP( (DWORD) 48000 );
				SetInputClock( ECHO_CLOCK_INTERNAL );
			}
		
			break;
			
		case DIGITAL_MODE_ADAT :
		
			//
			//	If the input clock is set to S/PDIF, set the 
			// input clock to internal and the sample rate to 48 KHz
			// 
			if ( ECHO_CLOCK_SPDIF == GetInputClock() )
			{
				m_pDspCommPage->dwSampleRate = SWAP( (DWORD) 48000 );
				SetInputClock( ECHO_CLOCK_INTERNAL );
			}
		
			//
			// If the current ASIC is the 96KHz ASIC, switch
			// the ASIC and set to 48 KHz
			//
			if ( ( DEVICE_ID_56361 == m_pOsSupport->GetDeviceId() && 
						pbMona1ASIC96_361 == m_pbyAsic ) ||
				  ( DEVICE_ID_56301 == m_pOsSupport->GetDeviceId() && 
						pbMona1ASIC96 == m_pbyAsic ) )
			{
				SetSampleRate( 48000 );	
			}

			dwControlReg |= GML_ADAT_MODE;
			dwControlReg &= ~GML_DOUBLE_SPEED_MODE;
			break;	
	}
	
	//
	// Write the control reg
	//
	WriteControlReg( dwControlReg );

	m_byDigitalMode = byNewMode;

	ECHO_DEBUGPRINTF( ("CMonaDspCommObject::SetDigitalMode to %ld\n",
							(DWORD) m_byDigitalMode) );

	return ECHOSTATUS_OK;
	
}	// ECHOSTATUS CMonaDspCommObject::SetDigitalMode


// **** CMonaDspCommObject.cpp ****
