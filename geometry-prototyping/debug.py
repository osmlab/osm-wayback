
import json

with open('tmp2','r') as inFile:
	for line in inFile:
		if len(line) > 10000:
			obj = json.loads(line)
			history = json.loads(obj['properties']['@history'])
			del obj['properties']['@history']
			print(obj['properties'])
			print("geometry type: ", obj['geometry']['type'])
			print("Points in coordinates: ", len(obj['geometry']['coordinates']))
			
			print("Number of history objects: ", len(history['objects'].keys()))
			print("--------------------\n")
