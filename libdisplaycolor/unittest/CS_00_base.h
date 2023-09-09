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

#ifndef __COLOR_SOLUTION_BASE_H__
#define __COLOR_SOLUTION_BASE_H__

#include <vector>
#include <unordered_map>
#include <map>
#include <cstring>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <stdio.h>
#include <fcntl.h>
#include <cutils/properties.h>
#include <math.h>
#include <unistd.h>
#include <gtest/gtest.h>

using std::string;
using std::vector;
using std::map;
using std::unordered_map;
using std::pair;
using std::make_pair;
using std::to_string;

class CS_00_base : public ::testing::Test {
  protected:
    void SetUpBase() {
        setMatrix(vector<float>());
        mLutMap.clear();
    }

    void TearDownBase() {
        mMatrix.clear();
        mLutMap.clear();
    }

    void setMatrix(const vector<float> matrix) {
        if (matrix.size()) {
            mMatrix = "1,";
            for (auto val : matrix)
                mMatrix += to_string(static_cast<int>(round(val * 65536))) + ",";
            mMatrix.pop_back();
        } else {
            mMatrix.clear();
        }
    }

    bool startSystemService(uint32_t timeout = 600) {
        sendCommand("start");
        return waitBootCompleted(1, timeout);
    }

    bool stopSystemService() {
        return sendCommand("stop");
    }

    bool sendCommand(string msg, int timeout = 60) {
        while (system(msg.c_str()) && timeout--)
            sleep(1);
        return timeout > 0 ? true : false;
    }

    bool isValidXml(string target) {
        xmlDocPtr doc;

        if (access(target.c_str(), F_OK) == -1)
            return false;

        doc = xmlParseFile(target.c_str());
        if (doc != NULL) {
            xmlFreeDoc(doc);
            return true;
        }

        return false;
    }

    string getXmlPath(string base, string prefix, bool hasSuffix = true) {
        string target;

        if (hasSuffix) {
            target = base + prefix + "_" + getSuffix();
            if (isValidXml(target))
                return target;
        }

        target = base + prefix + ".xml";
        if (isValidXml(target))
            return target;

        target.clear();
        return target;
    }

    void parseXml(uint32_t cm, uint32_t ri = 0) {
        string target = getXmlPath(XML_PATH, "calib_data_colormode0");
        auto GET_DOC_NAME_SUBXML = [=](string subxml) -> string {
            return (XML_PATH + "calib_data_colormode0" + "_" + subxml + ".xml");
        };

        xmlDocPtr doc;
        xmlNodePtr cur, node;

        ASSERT_FALSE(target.empty());
        ASSERT_TRUE(doc = xmlParseFile(target.c_str()));
        ASSERT_TRUE(cur = xmlDocGetRootElement(doc));
        ASSERT_EQ(0, xmlStrcmp(cur->name, (const xmlChar *)"Calib_Data"));

        cur = cur->xmlChildrenNode;

        while (cur != NULL) {
            xmlChar *gamut_id = NULL;
            xmlChar *intent_id = NULL;
            xmlChar *subXml = NULL;
            xmlDocPtr subdoc = NULL;

            if ((!xmlStrcmp(cur->name, (const xmlChar *)"mode"))) {
                uint32_t gId, iId;
                string temp_id;

                ASSERT_TRUE(xmlGetProp(cur, (const xmlChar *)"name"));
                ASSERT_TRUE(gamut_id = xmlGetProp(cur, (const xmlChar *)"gamut_id"));
                ASSERT_TRUE(intent_id = xmlGetProp(cur, (const xmlChar *)"intent_id"));

                temp_id = (const char*)gamut_id;
                gId = stoi(temp_id);
                temp_id = (const char*)intent_id;
                iId = stoi(temp_id);

                if (gId != cm || iId != ri)
                    goto next;

                node = cur->xmlChildrenNode;

                subXml = xmlGetProp(cur, (const xmlChar *)"subxml");
                if (subXml != NULL) {
                    string DOC_NAME_SUBXML = GET_DOC_NAME_SUBXML((const char*)subXml);

                    EXPECT_TRUE(subdoc = xmlParseFile(DOC_NAME_SUBXML.c_str()));
                    if (subdoc) {
                        node = parseSubXml(subdoc);
                        if (node == NULL)
                            node = cur->xmlChildrenNode;
                    }
                }

                mLutMap.clear();

                while (node != NULL) {
                    string nodeName = (const char*)node->name;
                    xmlChar *attr;
                    vector<int> attrs;
                    string tmp_att;
                    string tmp_data;
                    const char* att_nodes[] = {"att0", "att1", "att2", "att3"};

                    xmlChar *data;
                    data = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                    if (data == NULL)
                        goto next_node;

                    tmp_data = (const char*)data;
                    xmlFree(data);

                    attrs.clear();
                    for (auto att_node : att_nodes) {
                        attr = xmlGetProp(node, (const xmlChar*)att_node);
                        if (attr != NULL) {
                            tmp_att = (const char*)attr;
                            attrs.push_back(stoi(tmp_att));
                            xmlFree(attr);
                        } else
                            attrs.push_back(-1);
                    }

                    mLutMap.emplace(make_pair(nodeName, attrs), tmp_data);
next_node:
                    node = node->next;
                }
            }
next:
            cur = cur->next;

            if (gamut_id != NULL)
                xmlFree(gamut_id);
            if (intent_id != NULL)
                xmlFree(intent_id);
            if (subXml != NULL)
                xmlFree(subXml);
            if (subdoc != NULL)
                xmlFreeDoc(subdoc);
        }
        xmlFreeDoc(doc);
/*
        for(auto it = mLutMap.begin(); it != mLutMap.end(); ++it)
        {
            std::cout << " ---" << it->first.first << " "
                << (int)it->first.second[0] << "," << (int)it->first.second[1]
                << (int)it->first.second[2] << "," << (int)it->first.second[3]
                << std::endl;
        }
*/
    }

    virtual bool checkConfig(pair<string, vector<int>> lut, string &node, string &data) {
        bool ret = true;
	string nodename = lut.first;

        if (!nodename.compare("cgc17_enc")) {
            node = "cgc17_idx";
            data = to_string(lut.second[0]) + " " + to_string(lut.second[1]);
        } else if (!nodename.compare("hsc48_lcg")) {
            node = "hsc48_idx";
            data = to_string(lut.second[0]);
        } else if (!nodename.compare("gamma")) {
            node = "gamma_ext";
            data = to_string(lut.second[0] == 8 ? 1 : 0);
        } else if (!nodename.compare("degamma")) {
            node = "degamma_ext";
            data = to_string(lut.second[0] == 8 ? 1 : 0);
        } else {
            node.clear();
            data.clear();
            ret = false;
        }

        return ret;
    }

    virtual bool checkSkip(pair<string, vector<int>> __attribute__((unused)) lut) {
        return false;
    }

    virtual void checkData() {
        string data, node;

        for (auto lut = mLutMap.begin(); lut != mLutMap.end(); ++lut) {
            if (checkSkip(lut->first))
                continue;

            if (checkConfig(lut->first, node, data))
                putSysfs(node, data);

            getSysfs(lut->first.first, data);

            //EXPECT_EQ(lut->second.size(), data_s.length()) << " Error on " << lut->first.first;
            EXPECT_STRCASEEQ(lut->second.c_str(), data.c_str()) << " Error on " << lut->first.first;
        }

        if (mMatrix.length()) {
            getSysfs("gamma_matrix", data);

            //EXPECT_EQ(mMatrix.length(), data_s.length()) << " Error on gamma_matrix";
            EXPECT_STRCASEEQ(mMatrix.c_str(), data.c_str()) << " Error on gamma_matrix";
        }
    }

    map<pair<string, vector<int>>, string> mLutMap;
    string mMatrix;

    string XML_PATH = "/vendor/etc/dqe/";
    string XML_TUNE_PATH = "/data/dqe/";
    string SYSFS_PATH = "/sys/class/dqe/dqe/";
    string BYPASS_NAME = "Bypass";
    string BOOSTED_NAME = "Boosted";
    string NIGHTLIGHT_NAME = "NightLight";
    unordered_map<string, vector<float>> mTestMatrix = {
        {
            BYPASS_NAME,
            {
                1.000, 0.000, 0.000, 0.000,
                0.000, 1.000, 0.000, 0.000,
                0.000, 0.000, 1.000, 0.000,
                0.000, 0.000, 0.000, 1.000
            }
        },
        {
            BOOSTED_NAME,
            {
                1.079,-0.072,-0.007,0.000,
                -0.021, 1.028,-0.007,0.000,
                -0.021,-0.072, 1.093,0.000,
                0.000,  0.000, 0.000,1.000
            }
        },
        {
            NIGHTLIGHT_NAME,
            {
                1.000, 0.000, 0.000, 0.000,
                0.000, 0.723, 0.000, 0.000,
                0.000, 0.000, 0.459, 0.000,
                0.000, 0.000, 0.000, 1.000
            }
        }};

  private:
      bool waitBootCompleted(int onOff, uint32_t timeout = 600) {
          char value[PROPERTY_VALUE_MAX];

          while (timeout--) {
              property_get("service.bootanim.exit", value, "0");
              if (atoi(value) == onOff)
                  break;
              sleep(1);
          }
          return timeout > 0 ? true : false;
      }

      string getSuffix() {
          int fd;
          char output[128] = "\0";
          int rd_len;
          string suffix;
          string node = SYSFS_PATH + "xml";

          fd = (int)open(node.c_str(), O_RDONLY);
          if (fd < 0)
              return NULL;

          rd_len = read(fd, output, sizeof(output));
          close(fd);
          if (rd_len <= 0 || rd_len > sizeof(output))
              return NULL;

          output[rd_len] = '\0';
          suffix = output;
          suffix.pop_back();
          return suffix;
      }

      void getSysfs(string node, string &data) {
          int fd, len;
          char output[1024];
          string path = SYSFS_PATH + node;

          ASSERT_LE(0, fd = open(path.c_str(), O_RDONLY));
          memset(output, 0, sizeof(output));
          ASSERT_LT(0, len = read(fd, output, sizeof(output)));
          data = output;
          data = data.substr(0, data.rfind('\n'));
          close(fd);
      }

      void putSysfs(string node, string data) {
          int fd, len;
          string path = SYSFS_PATH + node;

          ASSERT_LE(0, fd = open(path.c_str(), O_WRONLY));
          ASSERT_LT(0, len = write(fd, data.c_str(), data.length()));
          close(fd);
      }

      xmlNodePtr parseSubXml(xmlDocPtr doc) {
          xmlNodePtr cur, node = NULL;

          cur = xmlDocGetRootElement(doc);
          if (cur == NULL) {
              return NULL;
          }

          if (xmlStrcmp(cur->name, (const xmlChar *)"Calib_Data")) {
              return NULL;
          }

          cur = cur->xmlChildrenNode;
          while (cur != NULL) {
              if ((!xmlStrcmp(cur->name, (const xmlChar *)"mode"))) {
                  xmlChar *mode_name;

                  mode_name = xmlGetProp(cur, (const xmlChar *)"name");
                  if ((!xmlStrcmp(mode_name, (const xmlChar *)"tune")))
                      node = cur->xmlChildrenNode;
                  if (mode_name != NULL)
                      xmlFree(mode_name);
              }
              cur = cur->next;
          }

          return node;
      }
};
#endif
