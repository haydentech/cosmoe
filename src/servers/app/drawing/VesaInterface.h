/*
 *		Bill Hayden <hayden@haydentech.com>
 */

#ifndef        __VESADRV_H__
#define        __VESADRV_H__

#include "BitmapHWInterface.h"
//#include <Region.h>	// for clipping_rect definition
#include "RGBColor.h"
#include <vector>
#include "vesa_defs.h"

#undef ScreenCount



struct VesaMode
{
			VesaMode( int w,
					  int h,
					  int bbl,
					  color_space cs,
					  int mode,
					  uint32 fb ) { m_nVesaMode = mode; m_nFrameBuffer = fb; }
	int		m_nVesaMode;
	uint32	m_nFrameBuffer;
};


class VesaInterface : public BitmapHWInterface {
public:
						VesaInterface();
	virtual				~VesaInterface();

	virtual	status_t		Initialize();

	// query for available hardware accleration and perform it
	// (Initialize() must have been called already)

	//virtual	status_t		Invalidate(const BRect& frame);

	int							screen;
	int							depth;
	unsigned long				window_mask;

protected:
	//bool					InitModes();
	//bool					SetVesaMode( uint32 nMode );

	sem_id					drawsem;

	int						m_nCurrentMode;
	uint32					m_nFrameBufferSize;
	int						m_nFrameBufferOffset;
	std::vector<VesaMode>	m_cModeList;

	ServerBitmap *_target;
};


#endif // __VESADRV_H__
