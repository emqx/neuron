*** Settings ***
Resource	api.resource
Resource	common.resource
Resource	error.resource

*** Keywords ***
Set a ${node} with right ${node_settings}, it will be success.
	[Tags]	Driver 	1

	${res}=	Node Setting  ${node}  ${node_settings}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

Set a ${node} with wrong ${node_settings}, it will be failure.
	[Tags]	Driver 	2

	${res}=	Node Setting  ${node}  ${node_settings}
	Check Response Status  ${res}  400
	Check Error Code  ${res}  ${NEU_ERR_NODE_SETTING_INVALID}

Add a ${tag} with right address and type and attribute from ${node}, it will be success.
	[Tags]	Driver 	3

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status 	${res} 	200
	Check Error Code 	${res} 	${NEU_ERR_SUCCESS}

	[Teardown] 	Del Group  ${node}  group

Set a ${tag} with unmatch address, type or attribute from ${node}, it will be failure and return ${resp_status} and ${error_code}.
	[Tags]	Driver 4
	
	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  ${resp_status}
	Check Error Code  ${res}  ${error_code}

	[Teardown] 	Del Group  ${node}  group

Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	[Tags]	Driver 	5

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	290ms
	${res}=	Read Tags  ${node}  group
	Check Response Status  ${res}  200

	Run Keyword	${check}	${res}[tags]	${tag_name}	${value}
	
	[Teardown]	Del Group	${node}	group

Read some ${tags} from ${node}, it will be success.
	[Tags]	Driver 6 and 7

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tags}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep 	290ms
	${res}=	Read Tags  ${node}  group
	Check Response Status  ${res}  200

	[Teardown]	Del Group  ${node}  group

Write and Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	[Tags]	Driver 	8

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	290ms
	${res}=	Write Tags  ${node}  group  ${tag_name}  ${value}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	100ms
	${res}=	Read Tags  ${node}  group
	Check Response Status  ${res}  200

	Run Keyword	${check}	${res}[tags]	${tag_name}	${value}

	[Teardown]	Del Group    ${node} 	group	

Write and read ${tag1} ${tag2} with same ${tag_name} but different address in different ${groups} that from ${node}, using ${check} to check the ${value}, it will be success.
	[Tags]	Driver 	9
	
	${res}=	Add Group  ${node}  group-1  100
	Check Response Status 	${res} 	200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Group  ${node}  group-2  100
	Check Response Status  ${res}  200
	Check Error Code   ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group-1  ${tag1}
	Check Response Status   ${res}  200
	Check Error Code 		${res} 	${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group-2  ${tag2}
	Check Response Status  ${res}  200
	Check Error Code 	${res} 	${NEU_ERR_SUCCESS}

	Sleep	290ms
	${res}=	Write Tags  ${node}  group-1  ${tag_name}  ${value}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	100ms
	${res}=	Read Tags  ${node}  group-1
	Check Response Status  ${res}  200
	Run Keyword	${check}	${res}[tags]	${tag_name}	${value}

	Sleep	290ms
	${res}=	Write Tags  ${node}  group-2  ${tag_name}  ${value}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	100ms
	${res}=	Read Tags  ${node}  group-1
	Check Response Status  ${res}  200
	Run Keyword	${check}	${res}[tags]	${tag_name}	${value}

	Del Group	${node}	group-1
	Del Group	${node}	group-2

Write and read a ${tag} with same ${tag_name} and same address in different ${groups} that from ${node}, using ${check} to check the ${value}, it will be success.
	[Tags]	Driver 	10

	FOR	${group}	IN	@{groups}
		${res}=	Add Group  ${node}  ${group}  100
		Check Response Status  ${res}  200
		Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

		${res}=	Add Tags  ${node}  ${group}  ${tag}
		Check Response Status  ${res}  200
		Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

		Sleep	290ms
		${res}=	Write Tags  ${node}  ${group}  ${tag_name}  ${value}
		Check Response Status  ${res}  200
		Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

		Sleep	100ms
		${res}=	Read Tags  ${node}  ${group}
		Check Response Status  ${res}  200
		Run Keyword	${check}	${res}[tags]	${tag_name}	${value}
	END

	FOR	${group}	IN	@{groups}
		Del Group  ${node}  ${group}
	END

Update a ${tag} by a ${tag_update} named ${tag_name} from ${node} , it can read and write, using ${check} to check the ${value}, it will be success.
	[Tags]	Driver 	12
	
	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  200
	Check Error Code 	${res} 	${NEU_ERR_SUCCESS}

	${res}=	Update Tags  ${node}  group  ${tag_update}
	Check Response Status  ${res}  200
	Check Error Code 	${res} 	${NEU_ERR_SUCCESS}

	${res}=	Write Tags  ${node}  group  ${tag_name}  ${value}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep 	290ms
	${res}=	Read Tags  ${node}  group
	Check Response Status  ${res}  200

	Run Keyword	${check}	${res}[tags]	${tag_name}	${value}

	[Teardown]	Del Group	${node}	group

Update a ${tag} by a ${tag_update} with the unmatch settings from ${node} , it will be failure and return ${resp_status} and ${error_code}.
	[Tags]	Driver 	13
	
	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  200
	Check Error Code 	${res} 	${NEU_ERR_SUCCESS}

	${res}=	Update Tags  ${node}  group  ${tag_update}
	Check Response Status  ${res}  ${resp_status}
	Check Error Code 	${res} 	${error_code}

	[Teardown]	Del Group  ${node}  group

Write a ${value} to a ${tag} with named ${tag_name} that don't exist in neuron or plc from ${node}, it will be failure.
	[Tags]	Driver 	14

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Write Tags  ${node}  group  ${tag_name}  ${value}
	Check Response Status 	${res} 	404
	Check Error Code 	${res} 	${NEU_ERR_TAG_NOT_EXIST}

	[Teardown]	Del Group  ${node}  group

Read a ${tag} with named ${tag_name} that don't exist in plc form ${node}, it will be failure ${error}.
	[Tags]	Driver 	15

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	290ms
	${res}=	Read Tags  ${node}  group
	Check Response Status 	${res} 	200
	Check Tag Error As Int  ${res}[tags]  ${tag_name}  ${error}

	[Teardown]	Del Group  ${node}  group

Write a ${value} to a ${tag} named ${tag_name} that without write attribute from ${node}, it will be failure.
	[Tags]	Driver 	16

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Write Tags  ${node}  group  ${tag_name}  ${value}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_PLUGIN_TAG_NOT_ALLOW_WRITE}

	[Teardown]	Del Group  ${node}  group

Write and read a ${value} to a ${tag} named ${tag_name} when the ${node} is stoped, it will be failure.
	[Tags]	Driver 	17

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status 	${res} 	200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Node Ctl  ${node}  ${NODE_CTL_STOP}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Write Tags  ${node}  group  ${tag_name}  ${value}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_PLUGIN_NOT_RUNNING}

	${res}=	Read Tags  ${node}  group
	Check Response Status  ${res}  200
	Check Tag Error As Int  ${res}[tags]  ${tag_name}  ${NEU_ERR_PLUGIN_NOT_RUNNING}

	[Teardown]	Del Group  ${node}  group

Write and Read ${tag_data1}, ${tag_data2} and ${tag_data3} named ${tag1}, ${tag2} and ${tag3} from ${node}, using ${check} to check the ${value1}, ${value2} and ${value3}, it will be success.
	[Tags]	Driver 	18

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag_data1}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag_data2}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag_data3}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	290ms
	${res}=	Write 3 Tags 1 Time  ${node}  group  ${tag1}  ${value1}  ${tag2}  ${value2}  ${tag3}  ${value3}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	100ms
	${res}=	Read Tags  ${node}  group
	Check Response Status  ${res}  200

	Run Keyword	${check}	${res}[tags]	${tag1}	${value1}
	Run Keyword	${check}	${res}[tags]	${tag2}	${value2}
	Run Keyword	${check}	${res}[tags]	${tag3}	${value3}

	[Teardown]	Del Group    ${node} 	group	