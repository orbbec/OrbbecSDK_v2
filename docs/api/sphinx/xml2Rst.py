#!/usr/bin/python
# -*- coding: UTF-8 -*-

# 本脚本用于,当C API头文件有更新，将C语言API xml自动生成./reference/c_ref.rst，并且把版本号更新到./index.rst
# C++ rst文件是手动编写的，如有C++头文件有更新，请修改./reference/cxx_ref.rst
# 首先工程build Doxygen,会在build路径生成xml
# 在当前目录执行 python.exe xml2Rst.py xml绝对路径，当前目录生成index.rst，reference目录生成c_ref.rst
# 如python.exe xml2Rst.py D:\obcode\SensorSDK_202111ref\build\Win_X64\release\xmlCN\
# 如果build目录有html文件，先删除，再执行bulid Doc

import os
import sys
from fileinput import filename
from xml.dom.minidom import parse
import xml.dom.minidom

keyWordsList = []
functionList = []
typedefList = []
enumList = []
defineList = []
structList = []

# C 头文件
fileNameList = ["Context.h","Device.h","Error.h","Filter.h","Frame.h","ObTypes.h","Pipeline.h","Property.h",
    "RecordPlayback.h","Sensor.h","StreamProfile.h","Version.h"]

# 接口已弃用
excludeFunctions =["ob_frame_width","ob_frame_height","ob_delete_frame_set",
    "ob_stream_profile_fps","ob_stream_profile_width","ob_stream_profile_height"]

# 获取index.xml节点信息
def getInfoFromXml(dirName):
    fileName= dirName + "\index.xml"
    domTree = xml.dom.minidom.parse(fileName)
    collection = domTree.documentElement
    keyWords = collection.getElementsByTagName("compound")     
    for keyWord in keyWords:
        if keyWord.getAttribute('kind')=="file":
            fileName = keyWord.getElementsByTagName('name')[0].childNodes[0].data
            if fileName in fileNameList:
                members = keyWord.getElementsByTagName('member')
                for member in members:
                    kind = member.getAttribute('kind')
                    if kind == "function":
                        name = member.getElementsByTagName('name')[0].childNodes[0].data
                        functionList.append(name)
                    elif kind == "typedef":
                        name = member.getElementsByTagName('name')[0].childNodes[0].data
                        typedefList.append(name)
                    elif kind == "enum":
                        name = member.getElementsByTagName('name')[0].childNodes[0].data
                        enumList.append(name)
                    elif kind == "define":
                        name = member.getElementsByTagName('name')[0].childNodes[0].data
                        defineList.append(name)
        elif keyWord.getAttribute('kind')=="struct":
            name = keyWord.getElementsByTagName('name')[0].childNodes[0].data
            structList.append(name)

# Doxyfile.xml节点信息
def getVersionFromXml(dirName):
    fileName= dirName + "\Doxyfile.xml"
    domTree = xml.dom.minidom.parse(fileName)
    collection = domTree.documentElement
    keyWords = collection.getElementsByTagName("option")    
    for keyWord in keyWords:
        if keyWord.getAttribute('id')=="PROJECT_NUMBER":
            global version
            version = keyWord.getElementsByTagName('value')[0].childNodes[0].data
            
            
# 根据xml的信息生成rst文件
def writeToRst():
    indexRst = open("./index.rst", "w")
    indexRst.write("OrbbecSdk v%s\n================\n\n" %version)
    indexRst.write(".. toctree::\n")
    indexRst.write("   :maxdepth: 3\n\n")
    indexRst.write("   summary/index\n")
    indexRst.write("   env/index\n")
    indexRst.write("   sample/index\n")    
    indexRst.write("   knowledge/index\n")    
    indexRst.write("   support/index\n")    
    indexRst.write("   reference/index\n")    
    indexRst.write("   orbbecviewer/index\n")    
    indexRst.close()

    crefRst = open("./reference/c_ref.rst", "w")
    crefRst.write("C Reference                                \n============================================\n")
    crefRst.write("Macros                                     \n--------------------------------------------\n")
    for item in defineList:
        crefRst.write(".. doxygendefine:: %s\n   :project: OrbbecSdk\n\n" %(item))
    crefRst.write("Structures                                 \n--------------------------------------------\n")
    for item in structList:
        crefRst.write(".. doxygenstruct:: %s\n   :project: OrbbecSdk\n\n" %(item))
    crefRst.write("Enumerations                               \n--------------------------------------------\n")
    for item in enumList:
        crefRst.write(".. doxygenenum:: %s\n   :project: OrbbecSdk\n\n" %(item))
    crefRst.write("Typedefs                                   \n--------------------------------------------\n")
    for item in typedefList:
        crefRst.write(".. doxygentypedef:: %s\n   :project: OrbbecSdk\n\n" %(item))
    crefRst.write("Functions                                  \n--------------------------------------------\n")
    for item in functionList:
        if item not in excludeFunctions:
            crefRst.write(".. doxygenfunction:: %s\n   :project: OrbbecSdk\n\n" %(item))
    crefRst.close()

try:
    xmlDir = sys.argv[1]
except:
    print("请传递xml的路径，如python.exe xml2Rst.py 路径")
else:
    getInfoFromXml(xmlDir)
    getVersionFromXml(xmlDir)
    writeToRst()
    print("生成./index.rst和./reference/c_ref.rst文件")



