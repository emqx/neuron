*** Settings ***

*** Variables ***

*** Test Cases ***
#Read unsubscribed point, it should return failure
#Read a point in the coil area, and the data as bit type, it should return success
#Read a point in the input area, and the data as bit type, it should return success
#Read a point in the input reg area, and the data as int16, it should return success
#Read a point in the input reg area, and the data as uint16, it should return success
#Read a point in the input reg area, and the data as uint32, it should return success
#Read a point in the input reg area, and the data as int32, it should return success
#Read a point in the input reg area, and the data as float, it should return success
#Read a point in the hlod reg area, and the data as int16, it should return success
#Read a point in the hlod reg area, and the data as uint16, it should return success
#Read a point in the hlod reg area, and the data as int32, it should return success
#Read a point in the hlod reg area, and the data as uint32, it should return success
#Read a point in the hlod reg area, and the data as float, it should return success
#Read multiple points in the coil area, the point address includes continuous and non-continuous, and the data as bit type, it should return success
#Read multiple points in the input area, the point address includes continuous and non-continuous, and the data as bit type, it should return success
#Read multiple points in the input reg area, the point address includes continuous and non-continuous, and the data includes int16/uint16/int32/uint32/float type, it should return success
#Read multiple points in the hold reg area, the point address includes continuous and non-continuous, and the data includes int16/uint16/int32/uint32/float type, it should return success
#Read multiple points belonging to different areas(coil/input/input reg/hold reg), the point address includes continuous and non-continuous, and the data include int16/uint16/int32/uint32/float type, it should return success
#Read multiple points belonging to different areas(coil/input/input reg/hold reg) and different sites, the point address includes continuous and non-continuous, and the data include int16/uint16/int32/uint32/float type, it should return success
#Write a point in the coil area, and the data as bit type, it should return success
#Write a point in the input area, and the data as bit type, it should return success
#Write a point in the input reg area, and the data as int16, it should return success
#Write a point in the input reg area, and the data as uint16, it should return success
#Write a point in the input reg area, and the data as uint32, it should return success
#Write a point in the input reg area, and the data as int32, it should return success
#Write a point in the input reg area, and the data as float, it should return success
#Write a point in the hlod reg area, and the data as int16, it should return success
#Write a point in the hlod reg area, and the data as uint16, it should return success
#Write a point in the hlod reg area, and the data as int32, it should return success
#Write a point in the hlod reg area, and the data as uint32, it should return success
#Write a point in the hlod reg area, and the data as float, it should return success

*** Keywords ***
Neuron Context Ready
	Neuron Ready