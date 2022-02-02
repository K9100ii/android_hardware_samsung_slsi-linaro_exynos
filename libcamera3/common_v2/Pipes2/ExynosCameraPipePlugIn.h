/*
**
** Copyright 2017, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef EXYNOS_CAMERA_PIPE_PLUG_IN_H
#define EXYNOS_CAMERA_PIPE_PLUG_IN_H

#include <list>
#include <map>

#include "ExynosCameraSWPipe.h"
#include "ExynosCameraFactoryPlugIn.h"

namespace android {

using namespace std;

typedef list<int>  keyList;

class ExynosCameraPipePlugIn : public ExynosCameraSWPipe {
private:
    class PlugInObject : public RefBase {
    public:
        PlugInObject(ExynosCameraPlugInSP_sptr_t plugIn, ExynosCameraPlugInConverterSP_sptr_t plugInConverter)
        {
            m_plugIn = plugIn;
            m_plugInConverter = plugInConverter;
        }

        virtual ~PlugInObject(){}

    public:
        ExynosCameraPlugInSP_sptr_t m_plugIn;
        ExynosCameraPlugInConverterSP_sptr_t m_plugInConverter;
    };

    typedef sp<PlugInObject>  PlugInSP_sptr_t; /* single ptr */
    typedef map<int, PlugInSP_sptr_t>  PlugInMap;
    typedef pair<int, PlugInSP_sptr_t>  PlugInPair;

public:
    ExynosCameraPipePlugIn()
    {
        m_init(NULL);
    }

    ExynosCameraPipePlugIn(
        int cameraId,
        ExynosCameraConfigurations *configurations,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums,
        int scenario = 0, bool supportMultiLibrary = false) : ExynosCameraSWPipe(cameraId, configurations, obj_param, isReprocessing, nodeNums)
    {
        m_scenario = scenario;
        m_supportMultiLibrary = supportMultiLibrary;
        m_init(nodeNums);
    }

    virtual ~ExynosCameraPipePlugIn();

    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        destroy(void);
    virtual status_t        start(void);
    virtual status_t        stop(void);
    virtual status_t        setupPipe(Map_t *map);
    virtual status_t        setParameter(int key, void *data);
    virtual status_t        getParameter(int key, void *data);

protected:
    typedef enum handle_status {
        PLUGIN_ERROR = -1,
        PLUGIN_NO_ERROR,
        PLUGIN_SKIP,
    } handle_status_t;
    virtual status_t                m_run(void);
    virtual handle_status_t         m_handleFrameBefore(ExynosCameraFrameSP_sptr_t frame, Map_t *map);
    virtual handle_status_t         m_handleFrameAfter(ExynosCameraFrameSP_sptr_t frame, Map_t *map);
private:
    void                    m_init(int32_t *nodeNums);
    status_t                m_create(int32_t *sensorIds = NULL);
    status_t                m_start(ExynosCameraPlugInSP_sptr_t plugIn);
    status_t                m_stop(ExynosCameraPlugInSP_sptr_t plugIn);
    status_t                m_setup(Map_t *map);
    status_t                m_setupPipe(ExynosCameraPlugInSP_sptr_t plugIn, ExynosCameraPlugInConverterSP_sptr_t plugInConverter, Map_t *map);
    status_t                m_pushItem(int key, PlugInMap *pluginList, Mutex *lock, PlugInSP_sptr_t item);
    status_t                m_popItem(int key, PlugInMap *pluginList, Mutex *lock, PlugInSP_sptr_t *item);
    status_t                m_getItem(int key, PlugInMap *pluginList, Mutex *lock, PlugInSP_sptr_t *item);
    bool                    m_findItem(int key, PlugInMap *pluginList, Mutex *lock);

    status_t                m_getKeyList(PlugInMap *pluginList, Mutex *lock, keyList *list);
    status_t                m_updateSetCmd(int cmd, void *data);
    status_t                m_updateGetCmd(int cmd, void *data);

    status_t                m_setScenario(int scenario);
    status_t                m_getScenario(int& scenario);

    bool                    m_loaderThreadFunc(void);

private:
    ExynosCameraFactoryPlugIn           *m_globalPlugInFactory;
    PlugInMap                            m_plugInList;
    mutable Mutex                        m_plugInListLock;

    ExynosCameraPlugInSP_sptr_t          m_plugIn;
    ExynosCameraPlugInConverterSP_sptr_t m_plugInConverter;
    Map_t                                m_map;
    bool                                 m_supportMultiLibrary;
    int                                  m_scenario;
    sp<ExynosCameraThread<ExynosCameraPipePlugIn>> m_loaderThread;
};

}; /* namespace android */

#endif
