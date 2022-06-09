*** Settings ***
Resource	api.resource
Resource	common.resource
Resource	error.resource

*** Keywords ***
Write and Read a ${tag} named ${tag_name} from ${node}, useing ${check} to check the ${value}, it will be success.
	${res}=		Add Group  ${node}  group  100
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	${res}=		Add Tags  ${node}  group  ${tag}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	300ms
	${res}=		Write Tags	${node}	group	${tag_name}	${value}
	Check Response Status  ${res}  200
	Check Error Code  ${res}  ${NEU_ERR_SUCCESS}

	Sleep	300ms
	${res}=		Read Tags	${node}		group
	Check Response Status  ${res}  200

	Run Keyword 	${check}	${res}[tags]	${tag_name}	${value}
	[Teardown]	Del Group  ${node}  group