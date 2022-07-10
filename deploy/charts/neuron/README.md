# neuron

![Version: 1.0.0](https://img.shields.io/badge/Version-1.0.0-informational?style=flat-square) ![Type: application](https://img.shields.io/badge/Type-application-informational?style=flat-square) ![AppVersion: 2.0.1](https://img.shields.io/badge/AppVersion-2.0.1-informational?style=flat-square)

A Helm chart for Kubernetes

## Install the Chart

- From Github
```
git clone https://github.com/emqx/neuron.git
cd deploy/charts/neuron
helm install my-neuron .
```

## Uninstall Chart
```
helm uninstall my-neuron
```

## Values

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| affinity | object | `{}` |  |
| autoscaling.enabled | bool | `false` |  |
| autoscaling.maxReplicas | int | `100` |  |
| autoscaling.minReplicas | int | `1` |  |
| autoscaling.targetCPUUtilizationPercentage | int | `80` |  |
| clusterDomain | string | `"cluster.local"` | Kubernetes Cluster Domain |
| containerPorts | object | `{"api":7001,"web":7000}` | Container Ports |
| containerPorts.api | int | `7001` | containerPorts.api |
| containerPorts.web | int | `7000` | containerPorts.web |
| fullnameOverride | string | `""` |  |
| hostPorts | object | `{"api":30371,"enabled":true,"web":30370}` | only for KubeEdge deployment |
| image.pullPolicy | string | `"IfNotPresent"` |  |
| image.repository | string | `"emqx/neuron"` |  |
| image.tag | string | `""` |  |
| imagePullSecrets | list | `[]` |  |
| ingress.annotations | object | `{}` |  |
| ingress.className | string | `""` |  |
| ingress.enabled | bool | `false` |  |
| ingress.hosts[0].host | string | `"chart-example.local"` |  |
| ingress.hosts[0].paths[0].path | string | `"/"` |  |
| ingress.hosts[0].paths[0].pathType | string | `"ImplementationSpecific"` |  |
| ingress.tls | list | `[]` |  |
| nameOverride | string | `""` |  |
| nodeSelector | object | `{}` |  |
| persistence.enabled | bool | `false` |  |
| persistence.existingClaim | string | `""` | Existing PersistentVolumeClaims The value is evaluated as a template So, for example, the name can depend on .Release or .Chart |
| podAnnotations | object | `{}` |  |
| readinessProbe.enabled | bool | `true` |  |
| readinessProbe.initialDelaySeconds | int | `5` |  |
| readinessProbe.periodSeconds | int | `5` |  |
| resources | object | `{}` |  |
| service.annotations | object | `{}` | Provide any additional annotations which may be required. Evaluated as a template |
| service.enabled | bool | `false` |  |
| service.loadBalancerSourceRanges | list | `[]` | loadBalancerSourceRanges: - 10.10.10.0/24  |
| service.nodePorts | object | `{"api":null,"web":null}` | Specify the nodePort(s) value for the LoadBalancer and NodePort service types. ref: https://kubernetes.io/docs/concepts/services-networking/service/#type-nodeport |
| service.portNames | object | `{"api":"api","web":"web"}` | Service ports name |
| service.portNames.api | string | `"api"` | service.portNames.api Neuron API port name |
| service.portNames.web | string | `"web"` | service.portNames.web Neuron Dashboard port name |
| service.ports | object | `{"api":7001,"web":7000}` | Service ports |
| service.ports.api | int | `7001` | service.ports.api Neuron API port |
| service.ports.web | int | `7000` | service.ports.web Neuron Dashboard port |
| service.type | string | `"ClusterIP"` |  |
| serviceAccount.annotations | object | `{}` | Annotations to add to the service account |
| serviceAccount.create | bool | `true` | Specifies whether a service account should be created |
| serviceAccount.name | string | `""` | The name of the service account to use. If not set and create is true, a name is generated using the fullname template |
| tolerations | list | `[]` |  |

