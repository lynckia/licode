#!/usr/bin/env python
import json
import ast

file = open("../../../../licode_config.js", "r") 
lines = file.readlines();
data = [];

for line in lines:
	if (line.find("config.erizo.installedHandlers") != -1):
		data = line.split('= ')[1]
		data
data = ast.literal_eval(data)		
file.close()


map = "{"
handlerEnum = "{"

for handler in data:
	jsonHandler = json.loads(handler)
	handlerName = jsonHandler["handlerName"]
	handlerClassName = jsonHandler["className"]
	map += ("{\"" + handlerName + "\" , " + handlerName + "Enum}," )
	handlerEnum += (handlerName + "Enum,")
map = map[0:-1] + "}"
handlerEnum = handlerEnum[0:-1] + "}"



file = open("HandlerImporter.h", "r") 
lines = file.readlines();
linesOutput = []

for line in lines:
	if (line.find("*handlerEnum*") != -1):
		linesOutput.append("\tenum HandlersEnum "+ handlerEnum + ";\n")
	elif (line.find("*handlerMap*") != -1):
		linesOutput.append("\tstd::map<std::string, HandlersEnum> handlersDic =" + map + ";\n")
	elif (line.find("*imports*") != -1):
		for handler in data:
			jsonHandler = json.loads(handler)
			handlerClassName = jsonHandler["className"]
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
		for handler in data:
			jsonHandler = json.loads(handler)
			handlerName = jsonHandler["handlerName"]
			handlerClassName = jsonHandler["className"]
			handlerEnum = (handlerName + "Enum")
			linesOutput.append("\t\tcase "+ handlerEnum +":\n")
			linesOutput.append("\t\t\tptr = std::make_shared<"+handlerClassName+">(parameters);\n")
			linesOutput.append("\t\t\tbreak;\n")

	else:
		linesOutput.append(line);
		
file = open("HandlerImporter.cpp", "w")
lines = file.writelines(linesOutput);
file.close()





