This program was made by Mitchell Hewitt for CS430 Computer Graphics (Section 1), Project 4 - Raytracer, in Fall 2016
This program reads in a height, width, json file, and output .ppm file.  
It parses the objects in the json file into a visual representation using raycasting and illumination.
The visual representation is then written out as an output p3 .ppm image file.

To use this program...

	1.  Compile it with the provided makefile (requires gcc).

	2.  Use the command "200 200 input.json output.ppm" to read the input json file
	     and write the objects illuminated within that json file to a p3 output.ppm 200x200 pixel image file.

If you would like to verify the raytracing...

	1.  Open output.ppm with software that can open .ppm (P3) files (e.g. GIMP)

	2.  Observe that the sphere is illuminated, and has light reflecting onto it from the plane beneath it.
		NOTE: No matter what epsilon I tried to use, the sphere came out grainy.

Invalid inputs and file contents will close the program.
This program is designed to use eight bits per color channel.