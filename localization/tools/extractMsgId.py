import os
import sys
import traceback
import PoRW

def _saveMsgId(msgList, fileName):
    file = open(fileName, "w", encoding="utf-8")
    for msg in msgList:
        if msg.msgCtxt != None:
            file.write(msg.msgCtxt)
        file.write(msg.msgId)
        file.write("\n\n")

if __name__ == "__main__":
    try:
        appDir = os.path.dirname(os.path.abspath(__file__))
        msgList, invalidMsgList = PoRW.readMsgList(sys.argv[1], False)
        if len(invalidMsgList) != 0:
            saveNameInvalid = os.path.join(appDir, "invalid.po")
            PoRW.saveMsgList(invalidMsgList, saveNameInvalid)
        _saveMsgId(msgList, "msgId.txt")
    except:
        traceback.print_exc()
        os.system('pause')
