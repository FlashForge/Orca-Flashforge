import os
import sys
import traceback
import PoRW

def _hasMsgStrPlural(msg):
    for line in msg.lines:
        lineTrimed = line.strip()
        if lineTrimed.startswith("msgstr[1]"):
            return True
    return False

def _getUnfinishedMsg(msgList):
    dstList = []
    for msg in msgList:
        if len(msg.msgStr) == 0:
            dstList.append(msg)
        elif _hasMsgStrPlural(msg) and len(msg.msgStrPlural) == 0:
            dstList.append(msg)
    return dstList

if __name__ == "__main__":
    try:
        appDir = os.path.dirname(os.path.abspath(__file__))
        msgList, invalidMsgList = PoRW.readMsgList(sys.argv[1], False)
        dstMsgList = _getUnfinishedMsg(msgList)
        if len(invalidMsgList) != 0:
            saveNameInvalid = os.path.join(appDir, "invalid.po")
            PoRW.saveMsgList(invalidMsgList, saveNameInvalid)
        saveNameDst = os.path.join(appDir, "unfinished.po")
        PoRW.saveMsgList(dstMsgList, saveNameDst)
    except:
        traceback.print_exc()
        os.system("pause")
