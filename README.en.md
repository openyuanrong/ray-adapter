# ray_adapter 

Compatible with the core interfaces of the open-source software Ray, it facilitates the seamless migration of workloads running on Ray (such as vllm/verl, etc.) to the openYuanrong cluster, while also enjoying the performance advantages brought by openYuanrong's deep optimization on Huawei Kunpeng and Ascend hardware.
For detailed installation and deployment instructions of openYuanrong, please refer to the official documentation: https://pages.openeuler.openatom.cn/openyuanrong/zh_cn/latest/index.html

## Installation

- Installation: `pip install ray_adapter`
- Deploy: See [Deploy](https://pages.openeuler.openatom.cn/openyuanrong/docs/zh-cn/latest/deploy/index.html)
- Usage: When using it, replace `import ray` in the application code with `import ray_adapter as ray` , and pay attention to the differences in the interfaces.

## Differences Between ray_adapter and ray Interfaces

| Interface Name              | Differences Compared to the Ray Interface                                            |
|-----------------------------|------------------------------------------------------------------------------------|
| `remote`                    | Supports parameter parsing. ray_adapter currently accepts the following parameters：`num_cpus`, `num_npus`, `resources`, `concurrency_groups`, `max_concurrency` |
| `method`                    | Currently, only the `num_returns` parameter is supported                            |
| `nodes`                     | Same to Ray                                                                          |
| `available_resources`       | Same to Ray                                                                          |
| `cluster_resources`         | Same to Ray                                                                          |
| `get`                       | Same to Ray                                                                          |
| `is_initialized`            | Same to Ray                                                                          |
| `init`                      | ray_adapter currently accepts the following parameters: `logging_level`, `num_cpus`, `runtime_env`                             |
| `kill`                      | Same to Ray                                                                          |
| `get_actor`                 | Returns a custom `ActorHandle` object，rather than the native Ray actor.             |
| `util.get_node_ip_address`  | Same to Ray                                                                          |
| `util.list_named_actors`    | Same to Ray                                                                          |
| `runtime_context().get_accelerator_ids` | Recorded NPU and device information                                      |  
| `runtime_context().get_node_id`         | The current output format is id + process                                |
| `runtime_context().namespace`           | Currently, the actor's namespace cannot be obtained. The ray_adapter currently defines the interface to return yr_default_namespace                |

### remote example

```python
import ray_adapter as ray
ray.init()
@ray.remote(num_cpus=1, max_retries=3)
def test_function():
    return "Hello!"
remote_function = test_function.remote()
result = ray.get(remote_function)
print(result)
ray.shutdown()
```

### nodes example

```python
import ray_adapter as ray
ray.init()
node_info = ray.nodes()
for node in node_info:
    print(node)
ray.shutdown()
```

### available_resources example

```python
import ray_adapter as ray
ray.init()
resources = ray.available_resources()
print(resources)
ray.shutdown()
```

### cluster_resources example

```python
import ray_adapter as ray
ray.init()
cluster = ray.cluster_resources()
print(cluster)
ray.shutdown()
```

### get example

```python
import ray_adapter as ray
ray.init()
@ray.remote()
def add(a, b):
    return a + b
obj_ref_1 = add.remote(1, 2)
obj_ref_2 = add.remote(3, 4)
result = ray.get([obj_ref_1, obj_ref_2], timeout=-1)
print(result)
ray.shutdown()
```

### is_initialized example

```python
import ray_adapter as ray
ray.init()
assert ray.is_initialized()
ray.shutdown()
assert not ray.is_initialized()
```

### method example

```python
import ray_adapter as ray
ray.init()
@ray.remote()
class f():
    @ray.method(num_returns=1)
    def method0(self):
        return 1


a = f.remote()
id = a.method0.remote()
print(ray.get(id))
ray.shutdown()
```

### init example

```python
import ray_adapter as ray
ray.init()
ray.shutdown()
```

### kill example

```python
import ray_adapter as ray
ray.init()
@ray.remote
class Actor:
    def fun(self):
        return 2
a = Actor.remote()
ray.kill(a)
```

### shutdown example

```python
import ray_adapter as ray
ray.init()
ray.shutdown()
```

### available_resources_per_node example

```python
import ray_adapter as ray
ray.init()
res = ray.available_resources_per_node()
print(res)
```

### get_actor example

```python
import ray_adapter as ray
ray.init()
@ray.remote
class Actor:
    def add(self):
        return 1

a = Actor.options(name='hi').remote()
print(f"res is {ray.get(a.add.remote())}")
ray.shutdown()
```

### util.get_node_ip_address example

```python
import ray_adapter as ray
ray.init()
node_ip = ray.util.get_node_ip_address()
print(node_ip)
```

### util.list_named_actors example

```python
import ray_adapter as ray
import time
ray.init()
@ray.remote
class A:
    pass
a = A.options(name="hi").remote()
time.sleep(2)
named_actors = ray.util.list_named_actors()
print(named_actors)
```

### runtime_context().get_accelerator_ids example

```python
import ray_adapter as ray
ray.init()
result = ray.runtime_context().get_accelerator_ids()
print(result)
```

### runtime_context().get_node_id example

```python
import ray_adapter as ray
ray.init()
result = ray.runtime_context().get_node_id()
print(result)
```

### runtime_context().namespace example

```python
import ray_adapter as ray
ray.init()
res = ray.get_runtime_context().namespace
print(res)
```
