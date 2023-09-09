#include <cstring>

#include "score.h"
#include "score_command.h"
#include "scv.h"

#define ABSDIFF(a, b) ((((a)-(b))<0)?(-((a)-(b))):((a)-(b)))

namespace score {

	sc_status_t scvFastCorners(ScBuffer* in1, ScBuffer* out, sc_s32 threshold, sc_enum_e corner_policy, sc_enum_e nms_policy)
	{
        sc_kernel_name_e kernel_id;

        if (nms_policy == SC_POLICY_NMS_USE) {
            kernel_id = SCV_FASTCORNERS_NMS_USE;
        } else if (nms_policy == SC_POLICY_NMS_NO_USE) {
            kernel_id = SCV_FASTCORNERS_NMS_NO_USE;
        }

		score::ScoreCommand cmd(kernel_id);

        if(nms_policy == SC_POLICY_NMS_USE) {
            ScBuffer* tmp = CreateScBuffer(out->buf.width, out->buf.height, out->buf.type);
            cmd.Put(in1);
            cmd.Put(tmp);
            cmd.Put(out);
            cmd.Put(&threshold);
            cmd.Put(&corner_policy);
            cmd.Put(&nms_policy);

            if (score::DoOnScore(SCV_FASTCORNERS_NMS_USE, cmd) < 0)
            {
                ReleaseScBuffer(tmp);
                return STS_FAILURE;
            }

            ReleaseScBuffer(tmp);
            return STS_SUCCESS;
        }else if(nms_policy == SC_POLICY_NMS_NO_USE) {
            cmd.Put(in1);
            cmd.Put(out);
            cmd.Put(&threshold);
            cmd.Put(&corner_policy);
            cmd.Put(&nms_policy);

            if (score::DoOnScore(SCV_FASTCORNERS_NMS_NO_USE, cmd) < 0)
            {
                return STS_FAILURE;
            }

            return STS_SUCCESS;
        }
        else {
            return STS_FAILURE;
        }
	}

}
