#!/usr/bin/env python
import json
import ast


from handlers import handlers 

map = "{"
handlerEnum = "{"

if (len(handlers)>0):
	for handler in handlers:
		handlerName = handler["handlerName"]
		handlerClassName = handler["className"]
		map += ("{\"" + handlerName + "\" , " + handlerName + "Enum}," )
		handlerEnum += (handlerName + "Enum,")
	map = map[0:-1] + "}"
	handlerEnum = handlerEnum[0:-1] + "}"
else: 
	map ="{}"
	handlerEnum = "{}"



file = open("HandlerImporter.h", "r") 
lines = file.readlines();
linesOutput = []

for line in lines:
	if (line.find("*handlerEnum*") != -1):
		linesOutput.append("\tenum HandlersEnum "+ handlerEnum + ";\n")
	elif (line.find("*handlerMap*") != -1):
		linesOutput.append("\tstd::map<std::string, HandlersEnum> handlersDic =" + map + ";\n")
	elif (line.find("*imports*") != -1):
		for handler in handlers:
			handlerClassName = handler["className"]
			linesOutput.append("#include \"" + handlerClassName + ".h\"\n")
	else:
		linesOutput.append(line)


file = open("HandlerImporter.h", "w")
lines = file.writelines(linesOutput);
file.close()


file = open("HandlerImporter.cpp", "r") 
lines = file.readlines()
linesOutput = []

for line in lines:
	if (line.find("*case*") != -1):
		for handler in handlers:
			handlerName = handler["handlerName"]
			handlerClassName = handler["className"]
			handlerEnum = (handlerName + "Enum")
			linesOutput.append("\t\tcase "+ handlerEnum +":\n")
			linesOutput.append("\t\t\tptr = std::make_shared<"+handlerClassName+">(parameters);\n")
			linesOutput.append("\t\t\tbreak;\n")

	else:
		linesOutput.append(line);
		
file = open("HandlerImporter.cpp", "w")
lines = file.writelines(linesOutput);
file.close()
