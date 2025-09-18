import os
import shutil
import sys
import traceback
import PoRW

class MyException(Exception):
    def __init__(self, message):
        super().__init__(message)

def _replaceMsgStr(orcaFilePath, replaceFilePath):
    orcaMsgList, orcaInvalidMsgList = PoRW.readMsgList(orcaFilePath, False, True)
    if len(orcaInvalidMsgList) != 0:
        raise(MyException(("bad input file %s") % orcaFilePath))
    replaceMsgList, replaceInvalidMsgList = PoRW.readMsgList(replaceFilePath, False)
    if len(replaceInvalidMsgList) != 0:
        raise(MyException(("bad input file %s") % orcaFilePath))
    orcaMsgMap = {}
    for msg in orcaMsgList:
        orcaMsgMap[PoRW.getMsgKey(msg)] = msg
    for msg in replaceMsgList:
        orcaMsg = orcaMsgMap.get(PoRW.getMsgKey(msg))
        if orcaMsg != None:
            orcaMsg.lines = msg.lines
        else:
            print("msgid not found: %s" % msg.msgId)
    PoRW.saveMsgList(orcaMsgList, orcaFilePath)

def _hasDupMsgId(orcaMsgList, ffmsgList):
    orcaMsgKeySet = set()
    for msg in orcaMsgList:
        orcaMsgKeySet.add(PoRW.getMsgKey(msg))
    ffMsgKeySet = set()
    for msg in ffmsgList:
        key = PoRW.getMsgKey(msg)
        if key in orcaMsgKeySet or key in ffMsgKeySet:
            print(key)
            return True
        ffMsgKeySet.add(key)
    return False

def _appendFile(orcaFilePath, ffFilePath, dstFilePath):
    file = open(dstFilePath, "w", encoding="utf-8")
    file.writelines(open(orcaFilePath, encoding="utf-8").readlines())
    file.write("\n")
    file.writelines(open(ffFilePath, encoding="utf-8").readlines())

def _appendPo(orcaFilePath, ffFilePath, dstFilePath):
    ffmsgList, ffInvalidMsgList = PoRW.readMsgList(ffFilePath, False)
    if len(ffInvalidMsgList) != 0:
        raise(MyException(("bad input file %s") % ffFilePath))
    orcaMsgList, orcaInvalidMsgList = PoRW.readMsgList(orcaFilePath, False)
    if _hasDupMsgId(orcaMsgList, ffmsgList):
        raise(MyException("duplicate msgid"))
    _appendFile(orcaFilePath, ffFilePath, dstFilePath)

if __name__ == "__main__":
    try:
        appDir = os.path.dirname(os.path.abspath(__file__))
        for lan in ["de", "en", "es", "fr", "ja", "ko", "lt", "zh_CN"]:
            orcaFileName = "OrcaSlicer_%s.po" % lan
            ffFileName = "flashforge_%s.po" % lan
            replaceFileName = "orca_%s.po" % lan
            dstFileName = "Orca-Flashforge_%s.po" % lan;
            orcaFilePath = os.path.join(appDir, "../i18n", lan, orcaFileName)
            ffFilePath = os.path.join(appDir, "../flashforge", lan, ffFileName)
            replaceFilePath = os.path.join(appDir, "../flashforge", lan, replaceFileName)
            dstFilePath = os.path.join(appDir, "../../resources/i18n", lan, dstFileName)
            moFilePath = os.path.join(appDir, "../../resources/i18n", lan, "Orca-Flashforge.mo")
            if lan in ["en", "es"]:
                _replaceMsgStr(orcaFilePath, replaceFilePath)
            if os.path.exists(orcaFilePath):
                _appendPo(orcaFilePath, ffFilePath, dstFilePath)
            else:
                shutil.copy(ffFilePath, dstFilePath)
    except MyException as e:
        print(e)
        os.system("pause")
    except:
        traceback.print_exc()
    os.system("pause")
