import os
import sys
import traceback
import PoRW

def _isFuzzyMsg(msg):
    findFuzzy = False
    findMsgId = False;
    for line in msg.lines:
        lineTrimed = line.lstrip()
        if lineTrimed.startswith("#,") and "fuzzy" in lineTrimed:
            findFuzzy = True
        elif lineTrimed.startswith("#|") and "msgid" in lineTrimed:
            findMsgId = True
    return findFuzzy and findMsgId

def _removeCommentAndMsgStr(msg):
    lines = []
    isMsgStr = False
    for line in msg.lines:
        lineTrimed = line.lstrip()
        if lineTrimed.startswith("#"):
            continue
        if lineTrimed.startswith("msgstr") and lineTrimed[6].isspace():
            lines.append("msgstr \"\"\n")
            isMsgStr = True
        elif lineTrimed.startswith("msgstr[0]"):
            lines.append("msgstr[0] \"\"\n")
            isMsgStr = True
        elif lineTrimed.startswith("msgstr[1]"):
            lines.append("msgstr[1] \"\"\n")
            isMsgStr = True
        elif lineTrimed.startswith("msgctxt")\
          or lineTrimed.startswith("msgid")\
          or lineTrimed.startswith("msgid_plural"):
            lines.append(line)
            isMsgStr = False
        elif not isMsgStr:
            lines.append(line)
    msg.msgStr = ""
    msg.lines = lines

def _removeFuzzyMsgStr(msgList):
    dstMsgList = []
    for msg in msgList:
        if _isFuzzyMsg(msg):
            _removeCommentAndMsgStr(msg)
        dstMsgList.append(msg)
    return dstMsgList

if __name__ == "__main__":
    try:
        appDir = os.path.dirname(os.path.abspath(__file__))
        msgList, invalidMsgList = PoRW.readMsgList(sys.argv[1], False)
        if len(invalidMsgList) != 0:
            saveNameInvalid = os.path.join(appDir, "invalid.po")
            PoRW.saveMsgList(invalidMsgList, saveNameInvalid)
        dstMsgList = _removeFuzzyMsgStr(msgList)
        PoRW.saveMsgList(dstMsgList, "removeFuzzy.po")
    except:
        traceback.print_exc()
        os.system('pause')
