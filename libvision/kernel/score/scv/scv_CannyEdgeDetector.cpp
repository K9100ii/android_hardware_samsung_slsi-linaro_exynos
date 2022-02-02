#include <cstring>

#include "score.h"
#include "score_command.h"
#include "scv.h"

namespace score {

	sc_status_t scvCannyEdgeDetector(ScBuffer* in, ScBuffer* out, sc_enum_e norm, sc_s32 th_low,	sc_s32 th_high)
	{
        score::ScoreCommand cmd(SCV_CANNYEDGEDETECTOR);

        sc_s32 image_width = in->buf.width;
        sc_s32 image_height = in->buf.height;
        data_buf_type in_type = in->buf.type;
        data_buf_type out_type = out->buf.type;

        if (!((in_type == SC_TYPE_U8)&&
                (out_type == SC_TYPE_U8))) {
            return STS_WRONG_TYPE;
        }

        if ((th_low & 0xffffff00) ||
                (th_high & 0xffffff00) ||
                !((sc_u8)th_low < (sc_u8)th_high)) {
            return STS_INVALID_VALUE;
        }

        ScBuffer *temp_buf_u8 = CreateScBuffer(image_width, image_height, SC_TYPE_U8, NULL);
        ScBuffer *temp_SobelX = CreateScBuffer(image_width, image_height, SC_TYPE_S16, NULL);
        ScBuffer *temp_SobelY = CreateScBuffer(image_width, image_height, SC_TYPE_S16, NULL);
        ScBuffer *temp_buf_u16 = CreateScBuffer(image_width, image_height, SC_TYPE_U16, NULL);
        ScBuffer *temp_postMag = CreateScBuffer(image_width, image_height, SC_TYPE_U8, NULL);

		cmd.Put(in);
		cmd.Put(out);
		cmd.Put(&norm);
		cmd.Put(&th_low);
		cmd.Put(&th_high);
		cmd.Put(temp_buf_u8);
		cmd.Put(temp_SobelX);
		cmd.Put(temp_SobelY);
		cmd.Put(temp_buf_u16);
		cmd.Put(temp_postMag);

		if (score::DoOnScore(SCV_CANNYEDGEDETECTOR, cmd) < 0)
		{
		    ReleaseScBuffer(temp_postMag);
            ReleaseScBuffer(temp_buf_u16);
            ReleaseScBuffer(temp_SobelY);
            ReleaseScBuffer(temp_SobelX);
            ReleaseScBuffer(temp_buf_u8);
			return STS_FAILURE;
		}

        ReleaseScBuffer(temp_postMag);
        ReleaseScBuffer(temp_buf_u16);
        ReleaseScBuffer(temp_SobelY);
        ReleaseScBuffer(temp_SobelX);
        ReleaseScBuffer(temp_buf_u8);

		return STS_SUCCESS;
	}

}
