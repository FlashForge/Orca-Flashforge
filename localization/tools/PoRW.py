class Msg(object):
    def __init__(self):
        self.msgCtxt = None
        self.msgId = None
        self.msgIdPlural = None
        self.msgStr = None
        self.msgStrPlural = None
        self.lines = []

def getMsgKeyRaw(msgctxt, msgid):
    if len(msgctxt) == 0:
        return msgid
    else:
        return msgctxt + "{+}" + msgid

def getMsgKey(msg):
    if msg.msgCtxt == None:
        return getMsgKeyRaw("", msg.msgId)
    else:
        return getMsgKeyRaw(msg.msgCtxt, msg.msgId)

def _procLineWithKey(lineVal, msg, attr):
    val = getattr(msg, attr)
    if val != None:
        return False
    if lineVal[0] != "\"" or lineVal[-1] != "\"":
        return False
    setattr(msg, attr, lineVal[1:-1])
    return True

def _parseMsg(msg, readVanished):
    attr = None
    for line in msg.lines:
        lineTrimed = line.strip()
        if readVanished and lineTrimed.startswith("#~"):
            lineTrimed = lineTrimed[2:].strip()
        if lineTrimed.startswith("msgctxt") and lineTrimed[7].isspace():
            if _procLineWithKey(lineTrimed[7:].strip(), msg, "msgCtxt"):
                attr = "msgCtxt"
            else:
                return False
        elif lineTrimed.startswith("msgid") and lineTrimed[5].isspace():
            if _procLineWithKey(lineTrimed[5:].strip(), msg, "msgId"):
                attr = "msgId"
            else:
                return False
        elif lineTrimed.startswith("msgid_plural") and lineTrimed[12].isspace():
            if _procLineWithKey(lineTrimed[12:].strip(), msg, "msgIdPlural"):
                attr = "msgIdPlural"
            else:
                return False
        elif lineTrimed.startswith("msgstr") and lineTrimed[6].isspace():
            if _procLineWithKey(lineTrimed[6:].strip(), msg, "msgStr"):
                attr = "msgStr"
            else:
                return False
        elif lineTrimed.startswith("msgstr[0]") and lineTrimed[9].isspace():
            if _procLineWithKey(lineTrimed[9:].strip(), msg, "msgStr"):
                attr = "msgStr"
            else:
                return False
        elif lineTrimed.startswith("msgstr[1]") and lineTrimed[9].isspace():
            if _procLineWithKey(lineTrimed[9:].strip(), msg, "msgStrPlural"):
                attr = "msgStrPlural"
            else:
                return False
        elif lineTrimed[0] == "\"" and lineTrimed[-1] == "\"":
            if attr == None:
                return False
            else:
                setattr(msg, attr, getattr(msg, attr) + lineTrimed[1:-1])
        elif lineTrimed.startswith("#"):
            attr = None
        else:
            return False
    return True

def _checkInvalidMsg(msg, readVanished, pos, fileName):
    allComments = True
    for line in msg.lines:
        lineTrimed = line.lstrip()
        if lineTrimed[0] != "#" or readVanished and lineTrimed[1] == "~":
            allComments = False
            break
    if allComments:
        return False
    if msg.msgId == "" and msg.msgStr != None:
        for line in msg.lines:
            if line.lstrip().startswith("\"Project-Id-Version:"):
                return
    print("invalid message %d, %s" % (pos, fileName))
    for line in msg.lines:
        print(line, end='')
    print()
    return True

def _procMsg(msg, fileName, readVanished, addCommentMsg, msgList, InvalidMsgList):
    if _parseMsg(msg, readVanished)\
    and msg.msgId != None and (len(msg.msgId) > 0 or addCommentMsg) and msg.msgStr != None:
        msgList.append(msg)
    elif _checkInvalidMsg(msg, readVanished, len(msgList), fileName):
        InvalidMsgList.append(msg)
    elif addCommentMsg:
        msgList.append(msg)

def readMsgList(fileName, readVanished, addCommentMsg = False):
    msg = Msg()
    msgList = []
    InvalidMsgList = []
    for line in open(fileName, encoding="utf-8").readlines():
        if len(line.strip()) == 0:
            if len(msg.lines) != 0:
                _procMsg(msg, fileName, readVanished, addCommentMsg, msgList, InvalidMsgList)
            msg = Msg()
        else:
            msg.lines.append(line)
    if len(msg.lines) != 0:
        _procMsg(msg, fileName, readVanished, addCommentMsg, msgList, InvalidMsgList)
    return msgList, InvalidMsgList

def saveMsgList(msgList, fileName):
    file = open(fileName, "w", encoding="utf-8")
    for i, msg in enumerate(msgList):
        file.writelines(msg.lines)
        if i != len(msgList) - 1:
            file.write("\n")
