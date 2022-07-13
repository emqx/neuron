*** Settings ***
Resource	api.resource
Resource	common.resource
Resource	error.resource

*** Keywords ***
Set a ${node} with right ${node_settings}, it will be success.
	[Tags] Driver 	1

	${res}=	Node Setting  ${node}  ${node_settings}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

Set a ${node} with wrong ${node_settings}, it will be failure.
	[Tags] Driver 	2

	${res}=	Node Setting  ${node}  ${node_settings}
	Check Response Status  ${res}  2004
	Check Error Code  ${res}  ${NEU_ERR_NODE_SETTING_INVALID}

Set a ${tag} with right address, type and attribute from ${node}, it will be success.
	[Tags] 	Driver 	3

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERROR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status 	${res} 	200
	Check Error Code 	${res} 	${NEU_ERR_SUCCESS}

	[Teardown] 	Del Group  ${node}  group

Set a ${tag} with unmatch address, type or attribute from ${node}, it will be failure and return ${resp_status} and ${error_code}.
	[Tags] 	Driver 4
	
	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  ${resp_status}
	Check Error Code  ${res}  ${error_code}

	[Teardown] 	Del Group  ${node}  group

Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	[Tags] 	Driver 	5

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	300ms
	${res}=	Read Tags  ${node}  group
	Check Response Status  ${res}  200

	Run Keyword	${check}	${res}[tags]	${tag_name}	${value}
	
	[Teardown]	Del Group	${node}	group

Read some ${tags} from ${node}, it will be success.
	[Tags] 	Driver 6 and 7

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	FOR 	${tag} 	IN 	@{tags}
		${res}=	Add Tags  ${node}  group  ${tag}
		Check Response Status 	${res} 	200
		Check Error Code  ${res}  ${NEU_ERR_SUCCESS}
	END

	Sleep 	1000ms
	${res}=	Read Tags  ${node}  group
	Check Response Status  ${res}  200

	[Teardown]	Del Group  ${node}  group

Write and Read a ${tag} named ${tag_name} from ${node}, using ${check} to check the ${value}, it will be success.
	[Tags] 	Driver 	8

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	300ms
	${res}=	Write Tags  ${node}  group  ${tag_name}  ${value}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	300ms
	${res}=	Read Tags  ${node}  group
	Check Response Status  ${res}  200

	Run Keyword	${check}	${res}[tags]	${tag_name}	${value}

	[Teardown]	Del Group    ${node} 	group	

Write and read same ${tag1} ${tag2} named same ${tag_name} but different address in different ${groups} that from ${node}, using ${check} to check the ${value}, it will be success.
	[Tags] 	Driver 	9
	
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

	Sleep	300ms
	${res}=	Write Tags  ${node}  group-1  ${tag_name}  ${value}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	300ms
	${res}=	Read Tags  ${node}  group-1
	Check Response Status  ${res}  200
	Run Keyword	${check}	${res}[tags]	${tag_name}	${value}

	Sleep	300ms
	${res}=	Write Tags  ${node}  group-2  ${tag_name}  ${value}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	300ms
	${res}=	Read Tags  ${node}  group-1
	Check Response Status  ${res}  200
	Run Keyword	${check}	${res}[tags]	${tag_name}	${value}

	Del Group	${node}	group-1
	Del Group	${node}	group-2

Write and read same a ${tag} with same ${tag_name} and same address in different ${groups} that from ${node}, using ${check} to check the ${value}, it will be success.
	[Tags] 	Driver 	10	
	
	${res}=	Add Group  ${node}  group-1  100
	Check Response Status 	${res} 	200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Group  ${node}  group-2  100
	Check Response Status  ${res}  200
	Check Error Code   ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group-1  ${tags}
	Check Response Status   ${res}  200
	Check Error Code 		${res} 	${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group-2  ${tags}
	Check Response Status  ${res}  200
	Check Error Code 	${res} 	${NEU_ERR_SUCCESS}

	Sleep	300ms
	${res}=	Write Tags  ${node}  group-1  ${tag_name}  ${value}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	300ms
	${res}=	Read Tags  ${node}  group-1
	Check Response Status  ${res}  200
	Run Keyword 	${check} 	${res}[tags] 	${tag_name} 	${value}

	Sleep	300ms
	${res}=	Read Tags  ${node}  group-2
	Check Response Status  ${res}  200
	Run Keyword 	${check} 	${res}[tags] 	${tag_name} 	${value}

	Del Group	${node}	group-1
	Del Group	${node}	group-2

Update a ${tag} by ${tag_update} named ${tag_name} that from ${node}, it can read and write, using  ${check} to check the ${value}.
	[Tags] 	Driver 	12
	
	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  200
	Check Error Code 	${res} 	${NEU_ERR_SUCCESS}

	${res}=	Update Tags  ${node}  group  ${tag_update}
	Check Response Status  ${res}  200
	Check Error Code 	${res} 	${NEU_ERR_SUCCESS}

	${res}=	Write Tags  ${node}  group  ${tag}  ${value}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep 	300ms
	${res}=	Read Tags  ${node}  group
	Check Response Status  ${res}  200
	Check Error Code 	${res} 	${NEU_ERR_SUCCESS}

	Run Keyword	$[check]	${res}[tags]	${tag_name}	${value}

	[Teardown]	Del Group	${node}	group

Update a ${tag} by a ${tag_update} with the unmatch settings from ${node} , it will be failure and return ${resp_status} and ${error_code}.
	[Tags] 	Driver 	13
	
	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  200
	Check Error Code 	${res} 	${NEU_ERR_SUCCESS}

	Sleep 	300ms
	${res}=	Update Tags  ${node}  group  ${tag_update}
	Check Response Status  ${res}  ${resp_status}
	Check Error Code 	${res} 	${error_code}

	[Teardown]	Del Group  ${node}  group

Write a ${value} to a tag with named ${tag_name} that don't exist in neuron from ${node}, it will be failure.
	[Tags] 	Driver 	14

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Write Tags  ${node}  group  ${tag_name}  ${value}
	Check Response Status 	${res} 	2201
	Check Error Code 	${res} 	${NEU_ERR_TAG_NOT_EXIST}

	[Teardown]	Del Group  ${node}  group

Read a tag with named ${tag_name} that don't exist in neuron form ${node}, using ${check} to check the ${value}, it will be failure.
	[Tags] 	Driver 	15

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Read Tags  ${node}  group
	Check Response Status 	${res} 	200
	Check Error Code 	${res} 	${NEU_ERR_SUCCESS}

	Run Keyword 	${check} 	${res}[tags] 	${tag_name} 	${value}

	[Teardown]	Del Group  ${node}  group

Write a ${value} to a ${tag} named ${tag_name} that without write attribute from ${node}, it will be failure.
	[Tags] 	Driver 	16

	${res}=	Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=	Write Tags  ${node}  group  ${tag_name}  ${value}
	Check Response Status  ${res}  2203
	Check Error Code  ${res}  ${NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT}

	[Teardown]	Del Group  ${node}  group

Write and read a ${value} to a ${tag} named ${tag_name} when the ${node} is stoped, it will be failure.
	[Tags] 	Driver 	17

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
	Check Response Status  ${res}  2009
	Check Error Code  ${res}  ${NEU_ERR_NODE_IS_STOPED}

	${res}=	Read Tags  ${node}  group
	Check Response Status  ${res}  2009
	Check Error Code  {$res}  ${NEU_ERR_NODE_IS_STOPED}

	[Teardown]	Del Group  ${node}  group
