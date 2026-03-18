# 简介

ray adapter 兼容了开源软件 Ray 的核心接口，可以将运行在 Ray 上的工作负载（如 vllm/verl 等）无缝迁移到 openYuanrong 集群上，享受 openYuanrong 在华为鲲鹏和昇腾硬件上深度优化带来的性能优势。

## 入门

- 安装：`pip install https://openyuanrong.obs.cn-southwest-2.myhuaweicloud.com/ray_adapter-0.5.0-py3-none-any.whl`。
- 部署：查看 openYuanrong 文档的[安装部署](https://docs.openyuanrong.org/zh-cn/latest/deploy/index.html)章节。
- 使用：将原来使用 Ray 开发的应用代码中的 `import ray` 替换为 `import ray_adapter as ray`，并关注接口的差异。

### ray adapter 接口与 ray 接口的差异说明

| 接口名称                    | 与 Ray接口 的差异                                                                        |
|-----------------------------|------------------------------------------------------------------------------------|
| `remote`                    | 支持参数解析，ray_adapter 目前支持参数有：`num_cpus`, `num_npus`, `resources`, `concurrency_groups`, `max_concurrency` |
| `method`                    | 目前仅支持`num_returns`参数                                                            |
| `nodes`                     | 相同                                                                                 |
| `available_resources`       | 相同                                                                                 |
| `cluster_resources`         | 相同                                                                                 |
| `get`                       | 相同                                                                                 |
| `is_initialized`            | 相同                                                                                 |
| `init`                      | 目前定义参数只有`logging_level`, `num_cpus`, `runtime_env`                             |
| `kill`                      | 相同                                                                                 |
| `get_actor`                 | 返回的是自定义 ActorHandle 对象，而非 Ray 原生 actor handle                              |
| `util.get_node_ip_address`  | 相同                                                                                 |
| `util.list_named_actors`    | 相同                                                                                 |
| `runtime_context().get_accelerator_ids` | 记录了 npu 和 device 信息                                                 |  
| `runtime_context().get_node_id`         | 目前输出效果为 id + 进程号                                                  |
| `runtime_context().namespace`           | 目前无法获取 actor 的命名空间，ray_adapter 目前定义接口返回空                   |

### remote 示例

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

### nodes 示例

```python
import ray_adapter as ray
ray.init()
node_info = ray.nodes()
for node in node_info:
    print(node)
ray.shutdown()
```

### available_resources 示例

```python
import ray_adapter as ray
ray.init()
resources = ray.available_resources()
print(resources)
ray.shutdown()
```

### cluster_resources 示例

```python
import ray_adapter as ray
ray.init()
cluster = ray.cluster_resources()
print(cluster)
ray.shutdown()
```

### get 示例

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

### is_initialized 示例

```python
import ray_adapter as ray
ray.init()
assert ray.is_initialized()
ray.shutdown()
assert not ray.is_initialized()
```

### method 示例

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

### init 示例

```python
import ray_adapter as ray
ray.init()
ray.shutdown()
```

### kill 示例

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

### shutdown 示例

```python
import ray_adapter as ray
ray.init()
ray.shutdown()
```

### available_resources_per_node 示例

```python
import ray_adapter as ray
ray.init()
res = ray.available_resources_per_node()
print(res)
```

### get_actor 示例

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

### util.get_node_ip_address 示例

```python
import ray_adapter as ray
ray.init()
node_ip = ray.util.get_node_ip_address()
print(node_ip)
```

### util.list_named_actors 示例

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

### util.placement_group

```python
import ray_adapter as ray
ray.init()
pg = ray.util.placement_group([{"CPU": 1, "GPU": 1}])
pg.wait()
ray.get(pg.ready(), timeout=10)
print(ray.util.placement_group_table(pg))
ray.util.remove_placement_group(pg)
```

### util.PlacementGroupSchedulingStrategy

```python
import ray_adapter as ray
from ray_adapter.util.scheduling_strategies import PlacementGroupSchedulingStrategy
ray.init()
pg = ray.util.placement_group([{"CPU": 2}])
ray.get(pg.ready())
@ray.remote(num_cpus=1)
def child():
    import time
    time.sleep(5)
@ray.remote(num_cpus=1)
def parent():
    ray.get(child.remote())
ray.get(
    parent.options(
        scheduling_strategy=PlacementGroupSchedulingStrategy(
            placement_group=pg
        )
    ).remote()
)
```

### util.NodeAffinitySchedulingStrategy

```python
import ray_adapter as ray
from ray_adapter.util.scheduling_strategies import NodeAffinitySchedulingStrategy
ray.init()
node_id = ray.runtime_context().get_node_id()
@ray.remote(num_cpus=1)
class Actor:
    def add(self, x):
        return x + 1
actor = Actor.options(scheduling_strategy=NodeAffinitySchedulingStrategy(node_id=node_id, soft=False)).remote()
ray.get(actor.add.remote(1))
```

### runtime_context().get_accelerator_ids 示例

```python
import ray_adapter as ray
ray.init()
result = ray.runtime_context().get_accelerator_ids()
print(result)
```

### runtime_context().get_node_id 示例

```python
import ray_adapter as ray
ray.init()
result = ray.runtime_context().get_node_id()
print(result)
```

### runtime_context().namespace 示例

```python
import ray_adapter as ray
ray.init()
res = ray.get_runtime_context().namespace
print(res)
```

## 贡献

我们欢迎您做各种形式的贡献，请参阅我们的[贡献者指南](https://docs.openyuanrong.org/zh-cn/latest/contributor_guide/index.html)。

## 许可证

[Apache License 2.0](./LICENSE)
