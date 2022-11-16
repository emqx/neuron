# neuron

![Version: 1.0.6](https://img.shields.io/badge/Version-1.0.6-informational?style=flat-square) ![Type: application](https://img.shields.io/badge/Type-application-informational?style=flat-square) ![AppVersion: 2.2.8](https://img.shields.io/badge/AppVersion-2.2.8-informational?style=flat-square)

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
| clusterDomain | string | `"cluster.local"` | Kubernetes Cluster Domain |
| fullnameOverride | string | `""` |  |
| hostPorts | object | `{"api":30371,"enabled":false,"web":30370}` | only for kubeedge deployment |
| image.pullPolicy | string | `"IfNotPresent"` |  |
| image.repository | string | `"emqx/neuron"` |  |
| image.tag | string | `""` | Overrides the image tag whose default is the chart appVersion. |
| imagePullSecrets | list | `[]` |  |
| nameOverride | string | `""` |  |
| neuronEnv.enabled | bool | `true` |  |
| neuronEnv.keys.disableAuth | string | `"DISABLE_AUTH"` |  |
| neuronEnv.values.disableAuth | bool | `true` |  |
| neuronMounts[0].capacity | string | `"100Mi"` |  |
| neuronMounts[0].mountPath | string | `"/opt/neuron/persistence"` |  |
| neuronMounts[0].name | string | `"pvc-neuron-data"` |  |
| nodeSelector | object | `{}` |  |
| persistence.accessMode | string | `"ReadWriteOnce"` |  |
| persistence.enabled | bool | `false` |  |
| persistence.storageClass | string | `"standard"` |  |
| podAnnotations | object | `{}` |  |
| resources | object | `{}` |  |
| service.annotations | object | `{"eip.openelb.kubesphere.io/v1alpha2":"eip-pool","lb.kubesphere.io/v1alpha1":"openelb","protocol.openelb.kubesphere.io/v1alpha1":"layer2"}` | Provide any additional annotations which may be required. Evaluated as a template |
| service.enabled | bool | `true` |  |
| service.loadBalancerSourceRanges | list | `[]` | loadBalancerSourceRanges: - 10.10.10.0/24  |
| service.nodePorts | object | `{"api":null,"web":null}` | Specify the nodePort(s) value for the LoadBalancer and NodePort service types. ref: https://kubernetes.io/docs/concepts/services-networking/service/#type-nodeport |
| service.ports | object | `{"api":{"name":"api","port":7001},"web":{"name":"web","port":7000}}` | Service ports |
| service.ports.api.name | string | `"api"` | Neuron API port name |
| service.ports.api.port | int | `7001` | Neuron API port |
| service.ports.web.name | string | `"web"` | Neuron Dashboard port name |
| service.ports.web.port | int | `7000` | Neuron Dashboard port |
| service.type | string | `"ClusterIP"` |  |
| tolerations[0].effect | string | `"NoExecute"` |  |
| tolerations[0].key | string | `"node.kubernetes.io/not-ready"` |  |
| tolerations[0].operator | string | `"Exists"` |  |
| tolerations[0].tolerationSeconds | int | `30` |  |
| tolerations[1].effect | string | `"NoExecute"` |  |
| tolerations[1].key | string | `"node.kubernetes.io/unreachable"` |  |
| tolerations[1].operator | string | `"Exists"` |  |
| tolerations[1].tolerationSeconds | int | `30` |  |