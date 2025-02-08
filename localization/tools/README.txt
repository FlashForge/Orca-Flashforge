注：lan 表示语言代码，比如 flashforge_lan.po 可以是 flashforge_de.po 或 flashforge_zh_CN.po

AutoConvertToXlsx.py
自动将 localization/flashforge/lan/flashforge_lan.po 转换为 excel 格式

AutoGetUnfinishedXlsx.py
自动将 localization/flashforge/lan/flashforge_lan.po 中未翻译的内容保存为 excel 格式

AutoMergeDestPo.py
自动将 localization/flashforge 中的 po 文件与 localization/i18n 中的 po 文件合并，并保存到 resources/i18n

AutoMergeXlsx.py
自动将脚本目录下 flashforge_lan.xlsx 文件中的翻译合并到 localization/flashforge/lan/flashforge_lan.po

convertPoToXlsx.py <po_file_path> <xlsx_file_path>
将 po 文件转换为 excel 格式

convertXlsxToPo.py <xlsx_file_path> <po_file_path>
将 excel 文件转换未 po 格式

diffByMsgId.py <lhs_po_file_path> <rhs_po_file_path>
查找 lhs_po_file_path 中 rhs_po_file_path 不存在的 msgid，并将这些差异的翻译保存到 diff.po

extractMsgId.py <po_file_path>
提取 po_file_path 中的 msgid 并保存到 msgId.txt

getUnfinishedMsg.py <po_file_path>
提取 po_file_path 中未翻译的内容，并保存到 unfinished.po

removeFuzzyMsgStr.py <po_file_path>
删除 po_file_path 中 PoEdit 自动猜测产生的翻译内容，并将新的翻译文件保存到 removeFuzzy.po
