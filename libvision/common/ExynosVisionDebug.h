/*
 * Copyright (C) 2015, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EXYNOS_OPENVX_DEBUG_H
#define EXYNOS_OPENVX_DEBUG_H


namespace android {

struct driver_node_time_stamp {
	vx_int64	driver_node_q;
	vx_int64	driver_node_dq;
	vx_int64	driver_node_start;
	vx_int64	drvier_node_end;
	vx_int64	firmware_start;
	vx_int64	firmware_end;	
};

struct vx_node_time_stmap {
    vx_int64    vx_node_start;
    vx_int64    vx_node_end;
    Vector<struct driver_node_time_stamp>   driver_node_info;
};

struct node_stats {
	vx_int64	tmp;
	vx_int64	beg;
	vx_int64	end;
	vx_int64	sum;
	vx_int64	avg;
	vx_int64	min;
	vx_int64	num;
	vx_int64	max;
}

class ExynosOpenVXDebug {
private:
    
public:

private:
    
public:

};

}; // namespace android
#endif
