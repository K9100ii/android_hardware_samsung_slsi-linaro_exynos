/*
 * Copyright 2022 The Android Open Source Project
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

#define LOG_TAG "CS_03_exynosdisplay"

#include <vector>
#include <map>
#include <cstring>
#include <hardware/exynos/libdisplaycolor.h>
#include <hardware/exynos/libdisplaycolor_drv.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <stdio.h>
#include <fcntl.h>
#include <cutils/properties.h>
#include "CS_00_base.h"

namespace android {
namespace hardware {
namespace exynos {
namespace {

using std::string;
using std::vector;
using std::map;
using std::unordered_map;
using std::pair;
using std::make_pair;
using std::to_string;

class CS_03_exynosdisplay : public CS_00_base {
  public:
    void SetUp() override {
        ASSERT_TRUE(startSystemService());
        SetUpBase();
    }

    void TearDown() override {
        TearDownBase();
    }

  protected:
      bool prepare(string base, string prefix, bool hasSuffix = true) {
          string xml_path = getXmlPath(base, prefix, hasSuffix);

          if (xml_path.empty())
              return false;

          parseXml(xml_path);
          return true;
    }

    bool prepare(string item) {
        int count = 2;

        EXPECT_NE(0, mTestSettings[item].size());

        if (!prepare(XML_PATH, mTestSettings[item][0], false))
            return false;

        do {
            EXPECT_TRUE(sendCommand("input keyevent WAKEUP"));
            EXPECT_TRUE(sendCommand("input keyevent KEYCODE_MENU"));
            EXPECT_TRUE(sendCommand("input keyevent KEYCODE_HOME"));
            EXPECT_TRUE(sendCommand(mTestSettings[item][1], 1));
        } while (!sendCommand(mTestSettings[item][2], 2) && --count);

        if (count <= 0) {
            std::cout << "---prepare error " << count << "---" << std::endl;
            return false;
        }

        sleep(3);
        return true;
    }

    void excute(string item, bool isService = true) {
        if (isService) {
            for (auto msg: mTestService[item].first)
                ASSERT_TRUE(sendCommand(msg, 5));
            sleep(1);
        } else {
            int count = 3;

            do {
                ASSERT_TRUE(sendCommand("input keyevent DPAD_DOWN"));
                ASSERT_TRUE(sendCommand("input keyevent DPAD_CENTER"));
                sleep(1);
            } while (!sendCommand(mTestSettings[item][3], 3) && count--);
            if (count <= 0)
                std::cout << "---excute error " << count << "---" << std::endl;
        }
    }

    void reset(string item, bool isService = true) {
        if (isService) {
            for (auto msg: mTestService[item].second)
                ASSERT_TRUE(sendCommand(msg, 5));
            sleep(1);
        } else {
            int count = 3;

            do {
                ASSERT_TRUE(sendCommand("input keyevent DPAD_CENTER"));
                sleep(1);
            } while (!sendCommand(mTestSettings[item][4], 3) && count--);
        }
    }

    virtual bool checkSkip(pair<string, vector<int>> lut) {
        unordered_map<string, int> subMap;
        vector<int> subIdx = {DQE_COLORMODE_ID_CGC17_ENC, DQE_COLORMODE_ID_HSC48_LCG,
                            DQE_COLORMODE_ID_GAMMA};
        bool ret = false;

        for (auto idx : subIdx)
            subMap.emplace(dqe_colormode_node[idx], idx);
        subMap.emplace("aps", DQE_COLORMODE_ID_MAX);

        switch (subMap[lut.first]) {
        case DQE_COLORMODE_ID_CGC17_ENC:
            if (findLutMap(dqe_colormode_node[DQE_COLORMODE_ID_CGC17_CON]).at(0) == '0')
                ret = true;
            break;
        case DQE_COLORMODE_ID_HSC48_LCG:
            if (findLutMap(dqe_colormode_node[DQE_COLORMODE_ID_HSC]).at(0) == '0')
                ret = true;
            break;
        case DQE_COLORMODE_ID_GAMMA:
            if (lut.second[2] != -1 && lut.second[2] != 7)
                ret = true;
            if (lut.second[3] != -1 && lut.second[3] != 3000)
                ret = true;
            break;
        case DQE_COLORMODE_ID_MAX: // for ATC
            if (lut.second[0] != 0)
                ret = true;
            break;
        }
        return ret;
    }

    string serviceMsg(string feature, int arg0, int arg1) {
        return "service call exynos_display 1 s16 " + feature +
                        " i32 "+ to_string(arg0) + " i32 "+ to_string(arg1) +
                        " > /dev/null";
    }

  private:
      void parseXml(string target) {
          xmlDocPtr doc;
          xmlNodePtr cur, node, subnode;

          ASSERT_FALSE(target.empty());
          ASSERT_TRUE(doc = xmlParseFile(target.c_str()));
          ASSERT_TRUE(cur = xmlDocGetRootElement(doc));
          ASSERT_STRCASEEQ((const char*)cur->name, "Calib_Data");

          cur = cur->xmlChildrenNode;

          while (cur != NULL) {
              if ((!xmlStrcmp(cur->name, (const xmlChar *)"mode"))) {
                  ASSERT_TRUE(xmlGetProp(cur, (const xmlChar *)"name"));
                  node = cur->xmlChildrenNode;

                  mLutMap.clear();

                  while (node != NULL) {
                      string nodeName = (const char*)node->name;
                      xmlChar *attr;
                      vector<int> attrs;
                      string tmp_att;
                      string tmp_data;
                      vector<string> att_nodes = {"att0", "att1", "index", "temp"};

                      if (nodeName.compare("atc") == 0) {
                          xmlChar *data;
                          subnode = node->xmlChildrenNode;
                          while (subnode != NULL) {
                              if (!xmlStrcmp(subnode->name, (const xmlChar *)"aps")) {
                                  data = xmlNodeListGetString(doc, subnode->xmlChildrenNode, 1);
                                  if (data != NULL) {
                                      tmp_data = (const char*)data;
                                      xmlFree(data);
                                      nodeName = (const char*)subnode->name;
                                      break;
                                  }
                              }
                              subnode = subnode->next;
                          }
                          att_nodes.clear();
                          att_nodes = {"al", "att1", "att2", "att3"};
                      } else {
                          xmlChar *data;
                          data = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                          if (data == NULL)
                              goto next_node;

                          tmp_data = (const char*)data;
                          xmlFree(data);
                      }

                      attrs.clear();

                      for (auto att_node : att_nodes) {
                          attr = xmlGetProp(node, (const xmlChar*)att_node.c_str());
                          if (attr != NULL) {
                              tmp_att = (const char*)attr;
                              attrs.push_back(stoi(tmp_att));
                              xmlFree(attr);
                          } else {
                              attrs.push_back(-1);
                          }
                      }

                      mLutMap.emplace(make_pair(nodeName, attrs), tmp_data);
                      next_node:
                      node = node->next;
                  }
              }
              cur = cur->next;
          }
          xmlFreeDoc(doc);
      }

      string findLutMap(string node) {
          for (auto lut = mLutMap.begin(); lut != mLutMap.end(); ++lut) {
              if (!node.compare(lut->first.first))
                  return lut->second;
          }
          return "";
      }

      unordered_map<string, pair<vector<string>, vector<string>>> mTestService = {
          {
              "atc",
              {
                  {
                      serviceMsg("atc_user", 2, 1),
                      serviceMsg("atc_user", 1, 0),
                  },
                  {
                      serviceMsg("atc_user", 2, 0),
                  }
              }
          },
          {
              "att",
              {
                  {
                      serviceMsg("atc_tune", 0, 1),
                      serviceMsg("atc_tune", 9, 0),
                  },
                  {
                      serviceMsg("atc_tune", 0, 0),
                  }
              }
          },
          {
              "qed",
              {
                  {
                      serviceMsg("dqe_tune", 0, 1),
                  },
                  {
                      serviceMsg("dqe_tune", 0, 0),
                  }
              }
          }};

      string AM_PREFIX = "am start -a android.settings.";
      string AM_POSTFIX = " --activity-clear-top > /dev/null";
      string DUMP_PREFIX = "dumpsys window | grep -e mFocusedWindow | grep -q ";
      string DUMP2_PREFIX = "dumpsys activity com.android.settings | grep -e ";
      string DUMP2_POSTFIX = " | grep -q GFED";
      string GET_AMCMD(string data) {return AM_PREFIX + data + AM_POSTFIX;};
      string GET_DUMPCMD(string data) {return DUMP_PREFIX + data;};
      string GET_DUMP2CMD(string data) {return DUMP2_PREFIX + data + DUMP2_POSTFIX;};
      unordered_map<string, vector<string>> mTestSettings = {
          {
              "gamma",
              {
                  "calib_data_eyetemp",
                  GET_AMCMD("EXYNOS_COLOR_GAMMA_SETTINGS"),
                  GET_DUMPCMD("ExynosColorGammaSettingsActivity"),
                  GET_DUMP2CMD("eye_temperature_turn_on_button"),
                  GET_DUMP2CMD("eye_temperature_turn_off_button"),
              }
          },
          {
              "hsc",
              {
                  "calib_data_skincolor",
                  GET_AMCMD("EXYNOS_COLOR_HSC_SETTINGS"),
                  GET_DUMPCMD("ExynosColorHSCSettingsActivity"),
                  GET_DUMP2CMD("skin_color_turn_on_button"),
                  GET_DUMP2CMD("skin_color_turn_off_button"),
              }
          },
          {
              "cgc",
              {
                  "calib_data_whitepoint",
                  GET_AMCMD("EXYNOS_COLOR_CGC_SETTINGS"),
                  GET_DUMPCMD("ExynosColorCGCSettingsActivity"),
                  GET_DUMP2CMD("white_point_turn_on_button"),
                  GET_DUMP2CMD("white_point_turn_off_button"),
              }
          },
          {
              "de",
              {
                  "calib_data_sharpness",
                  GET_AMCMD("EXYNOS_COLOR_DE_SETTINGS"),
                  GET_DUMPCMD("ExynosColorDESettingsActivity"),
                  GET_DUMP2CMD("edge_sharpness_turn_on_button"),
                  GET_DUMP2CMD("edge_sharpness_turn_off_button"),
              }
          }};
};

TEST_F(CS_03_exynosdisplay, CS_03_01_ServiceValidationTest) {
    ASSERT_TRUE(sendCommand("service list | grep -q exynos_display", 10));
}

TEST_F(CS_03_exynosdisplay, CS_03_02_AtcUserCallTest) {
    if (!prepare(XML_PATH, "calib_data_atc")) {
        GTEST_SKIP() << "Can't find xml files";
        return;
    }
    excute("atc");
    checkData();
    reset("atc");
}

TEST_F(CS_03_exynosdisplay, CS_03_03_ATTToolTest) {
    if (!prepare(XML_TUNE_PATH, "calib_data_atc", false)) {
        GTEST_SKIP() << "Can't find xml files";
        return;
    }
    excute("att");
    checkData();
    reset("att");
}

TEST_F(CS_03_exynosdisplay, CS_03_04_QEDToolTest) {
    if (!prepare(XML_TUNE_PATH, "calib_data", false)) {
        GTEST_SKIP() << "Can't find xml files";
        return;
    }
    excute("qed");
    checkData();
    reset("qed");
}

TEST_F(CS_03_exynosdisplay, CS_03_05_EyeProtectionTest) {
    if (!prepare("gamma")) {
        GTEST_SKIP() << "Activity not started or can't find xml file";
        return;
    }
    excute("gamma", 0);
    checkData();
    reset("gamma", 0);
}

TEST_F(CS_03_exynosdisplay, CS_03_06_SkinProtectionTest) {
    if (!prepare("hsc")){
        GTEST_SKIP() << "Activity not started or can't find xml file";
        return;
    }
    excute("hsc", 0);
    checkData();
    reset("hsc", 0);
}

TEST_F(CS_03_exynosdisplay, CS_03_07_WhitePointTest) {
    if (!prepare("cgc")){
        GTEST_SKIP() << "Activity not started or can't find xml file";
        return;
    }
    excute("cgc", 0);
    checkData();
    reset("cgc", 0);
}

TEST_F(CS_03_exynosdisplay, CS_03_08_EdgeSharpnessTest) {
    if (!prepare("de")){
        GTEST_SKIP() << "Activity not started or can't find xml file";
        return;
    }
    excute("de", 0);
    checkData();
    reset("de", 0);
}

TEST_F(CS_03_exynosdisplay, CS_03_09_RandomFuncParamTest) {
    int count = 100;

    std::cout << "---Testing Random Function/Parameter " << count << "---" << std::endl;
    while (count--) {
        vector<string> arg0 = {
                "setDisplayColorFeature",
                "dqe_tune",
                "hdr_tune",
                "atc_user",
                "atc_tune",
                "atc_timer",
                "factory_timer"};
        int arg1 = rand() % 100;
        int arg2 = rand() % 100;
        int idx = rand() % arg0.size();

        sendCommand(serviceMsg(arg0.at(idx), arg1, arg2));
        usleep(500);
    }
}

}  // anonymous namespace
}  // namespace exynos
}  // namespace hardware
}  // namespace android
