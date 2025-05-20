import os
import sys
import traceback
import PoRW

def _diffMsgList(lhs, rhs):
    msgKeySet = set()
    for msg in rhs:
        msgKeySet.add(PoRW.getMsgKey(msg))
    diffList = []
    for msg in lhs:
        if not PoRW.getMsgKey(msg) in msgKeySet:
            diffList.append(msg)
    return diffList

if __name__ == "__main__":
    try:
        appDir = os.path.dirname(os.path.abspath(__file__))
        msgList0, invalidMsgList0 = PoRW.readMsgList(sys.argv[1], False)
        msgList1, invalidMsgList1 = PoRW.readMsgList(sys.argv[2], True)
        dstMsgList = _diffMsgList(msgList0, msgList1)
        if len(invalidMsgList0) != 0:
            saveNameInvalidLhs = os.path.join(appDir, "lhsInvalid.po")
            PoRW.saveMsgList(invalidMsgList0, saveNameInvalidLhs)
        if len(invalidMsgList1) != 0:
            saveNameInvalidRhs = os.path.join(appDir, "rhsInvalid.po")
            PoRW.saveMsgList(invalidMsgList1, saveNameInvalidRhs)
        saveNameDst = os.path.join(appDir, "diff.po")
        PoRW.saveMsgList(dstMsgList, saveNameDst)
    except:
        traceback.print_exc()
        os.system("pause")
