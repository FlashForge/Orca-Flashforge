import os
import traceback
from convertPoToXlsx import convertPoToXlsx

if __name__ == '__main__':
    try:
        app_dir = os.path.dirname(os.path.abspath(__file__))
        for lan in ["de", "en", "es", "fr", "ja", "ko", "lt"]:
            poFileName = "flashforge_%s.po" % lan
            poFilePath = os.path.join(app_dir, "../flashforge", lan, poFileName)
            xlsxFilePath = os.path.join(app_dir, "flashforge_all_%s.xlsx" % lan)
            convertPoToXlsx(poFilePath, xlsxFilePath)
    except:
        traceback.print_exc()
        os.system("pause")
        