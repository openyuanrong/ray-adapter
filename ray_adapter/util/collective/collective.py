import logging
import os
import threading
from typing import List

import numpy as np

from ray_adapter.util.collective import types
from ray_adapter.worker import get_actor, get_gpu_ids

logger = logging.getLogger(__name__)

try:
    from ray_adapter.util.collective.collective_group.nccl_collective_group import NCCLGroup

    _NCCL_AVAILABLE = True
    _LOG_NCCL_WARNING = False
except ImportError:
    _NCCL_AVAILABLE = False
    _LOG_NCCL_WARNING = True


def nccl_available():
    global _LOG_NCCL_WARNING
    if get_gpu_ids() and _LOG_NCCL_WARNING:
        logger.warning(
            "NCCL seems unavailable. Please install Cupy "
            "following the guide at: "
            "https://docs.cupy.dev/en/stable/install.html."
        )
        _LOG_NCCL_WARNING = False
    return _NCCL_AVAILABLE


class GroupManager(object):
    """Use this class to manage the collective groups we created so far.

    Each process will have an instance of `GroupManager`. Each process
    could belong to multiple collective groups. The membership information
    and other metadata are stored in the global `_group_mgr` object.
    """

    def __init__(self):
        self._name_group_map = {}

    def create_collective_group(
        self, backend, world_size, rank, group_name, gloo_timeout
    ):
        """The entry to create new collective groups in the manager.

        Put the registration and the group information into the manager
        metadata as well.
        """
        backend = types.Backend(backend)

        if backend == types.Backend.NCCL:
            _check_backend_availability(backend)
            logger.debug("Creating NCCL group: '%s'...", group_name)
            g = NCCLGroup(world_size, rank, group_name)
        else:
            raise RuntimeError(f"Unexpected backend: {backend}")

        self._name_group_map[group_name] = g
        return self._name_group_map[group_name]

    def is_group_exist(self, group_name):
        return group_name in self._name_group_map

    def get_group_by_name(self, group_name):
        """Get the collective group handle by its name."""
        if not self.is_group_exist(group_name):
            logger.warning("The group '{}' is not initialized.".format(group_name))
            return None
        return self._name_group_map[group_name]


_group_mgr = GroupManager()
# This lock is used to make external calls to the _group_mgr thread-safe.
_group_mgr_lock = threading.Lock()


def init_collective_group(
    world_size: int,
    rank: int,
    backend=types.Backend.NCCL,
    group_name: str = "default",
    gloo_timeout: int = 30000,
):
    """Initialize a collective group inside an actor process.

    Args:
        world_size: the total number of processes in the group.
        rank: the rank of the current process.
        backend: the CCL backend to use, NCCL or GLOO.
        group_name: the name of the collective group.

    Returns:
        None
    """
    # _check_inside_actor()
    backend = types.Backend(backend)
    _check_backend_availability(backend)
    global _group_mgr
    global _group_mgr_lock

    if not group_name:
        raise ValueError("group_name '{}' needs to be a string.".format(group_name))

    with _group_mgr_lock:
        if _group_mgr.is_group_exist(group_name):
            raise RuntimeError("Trying to initialize a group twice.")

        assert world_size > 0
        assert rank >= 0
        assert rank < world_size
        _group_mgr.create_collective_group(
            backend, world_size, rank, group_name, gloo_timeout
        )


def create_collective_group(
    actors,
    world_size: int,
    ranks: List[int],
    backend=types.Backend.NCCL,
    group_name: str = "default",
    gloo_timeout: int = 30000,
):
    """Declare a list of actors as a collective group.

    Note: This function should be called in a driver process.

    Args:
        actors: a list of actors to be set in a collective group.
        world_size: the total number of processes in the group.
        ranks (List[int]): the rank of each actor.
        backend: the CCL backend to use, NCCL or GLOO.
        group_name: the name of the collective group.

    Returns:
        None
    """
    backend = types.Backend(backend)
    _check_backend_availability(backend)

    name = "info_" + group_name
    try:
        get_actor(name)
        raise RuntimeError("Trying to initialize a group twice.")
    except ValueError:
        pass

    if len(ranks) != len(actors):
        raise RuntimeError(
            "Each actor should correspond to one rank. Got '{}' "
            "ranks but '{}' actors".format(len(ranks), len(actors))
        )

    if set(ranks) != set(range(len(ranks))):
        raise RuntimeError(
            "Ranks must be a permutation from 0 to '{}'. Got '{}'.".format(
                len(ranks), "".join([str(r) for r in ranks])
            )
        )

    if world_size <= 0:
        raise RuntimeError(
            "World size must be greater than zero. Got '{}'.".format(world_size)
        )
    if not all(ranks) >= 0:
        raise RuntimeError("Ranks must be non-negative.")
    if not all(ranks) < world_size:
        raise RuntimeError("Ranks cannot be greater than world_size.")

    # avoid a circular dependency
    from ray_adapter.util.collective.util import Info

    # store the information into a NamedActor that can be accessed later.
    name = "info_" + group_name
    actors_id = [a._actor_id for a in actors]
    info = Info.options(name=name, lifetime="detached").remote()
    import ray_adapter
    ray_adapter.get([info.set_info.remote(actors_id, world_size, ranks, backend, gloo_timeout)])



def _check_inside_actor():

    """Check if currently it is inside a Ray actor/task."""
    return


def _check_backend_availability(backend: types.Backend):
    """Check whether the backend is available."""

    if backend == types.Backend.NCCL:
        if not nccl_available():
            raise RuntimeError("NCCL is not available.")


def broadcast(tensor, src_rank: int = 0, group_name: str = "default"):
    """Broadcast the tensor from a source process to all others.

    Args:
        tensor: the tensor to be broadcasted (src) or received (destination).
        src_rank: the rank of the source process.
        group_name: the collective group name to perform broadcast.

    Returns:
        None
    """
    _check_single_tensor_input(tensor)
    g = get_group_handle(group_name)

    # check src rank
    _check_rank_valid(g, src_rank)
    opts = types.BroadcastOptions()
    opts.root_rank = src_rank
    opts.root_tensor = 0
    g.broadcast([tensor], opts)


def barrier(group_name: str = "default"):
    """Barrier all processes in the collective group.

    Args:
        group_name: the name of the group to barrier.

    Returns:
        None
    """
    g = get_group_handle(group_name)
    g.barrier()


def send(tensor, dst_rank: int, group_name: str = "default"):
    """Send a tensor to a remote process synchronously.

    Args:
        tensor: the tensor to send.
        dst_rank: the rank of the destination process.
        group_name: the name of the collective group.

    Returns:
        None
    """
    _check_single_tensor_input(tensor)
    g = get_group_handle(group_name)
    _check_rank_valid(g, dst_rank)
    if dst_rank == g.rank:
        raise RuntimeError("The destination rank '{}' is self.".format(dst_rank))
    opts = types.SendOptions()
    opts.dst_rank = dst_rank
    g.send([tensor], opts)


def recv(tensor, src_rank: int, group_name: str = "default"):
    """Receive a tensor from a remote process synchronously.

    Args:
        tensor: the received tensor.
        src_rank: the rank of the source process.
        group_name: the name of the collective group.

    Returns:
        None
    """
    _check_single_tensor_input(tensor)
    g = get_group_handle(group_name)
    _check_rank_valid(g, src_rank)
    if src_rank == g.rank:
        raise RuntimeError("The destination rank '{}' is self.".format(src_rank))
    opts = types.RecvOptions()
    opts.src_rank = src_rank
    g.recv([tensor], opts)


def get_group_handle(group_name: str = "default"):
    """Check if the group is initialized and return the group handle.

    Args:
        group_name: the name of the collective group.

    Returns:
        The collective group handle.
    """
    _check_inside_actor()
    global _group_mgr
    global _group_mgr_lock
    with _group_mgr_lock:
        if not _group_mgr.is_group_exist(group_name):
            # try loading from remote info store
            try:
                # if the information is stored in an Info object,
                # get and create the group.
                name = "info_" + group_name
                import ray_adapter as ray
                mgr = get_actor(name=name)
                ids, world_size, rank, backend, gloo_timeout = ray.get(
                    mgr.get_info.remote()
                )
                worker = ray._private.worker.global_worker
                id_ = worker.core_worker.get_actor_id()
                r = rank[ids.index(id_)]
                _group_mgr.create_collective_group(
                    backend, world_size, r, group_name, gloo_timeout
                )
            except ValueError as exc:
                # check if this group is initialized using options()
                if (
                    "collective_group_name" in os.environ
                    and os.environ["collective_group_name"] == group_name
                ):
                    rank = int(os.environ["collective_rank"])
                    world_size = int(os.environ["collective_world_size"])
                    backend = os.environ["collective_backend"]
                    gloo_timeout = os.getenv("collective_gloo_timeout", 30000)
                    _group_mgr.create_collective_group(
                        backend, world_size, rank, group_name, gloo_timeout
                    )
                else:
                    raise RuntimeError(
                        "The collective group '{}' is not "
                        "initialized in the process.".format(group_name)
                    ) from exc
        g = _group_mgr.get_group_by_name(group_name)
        return g


def _check_single_tensor_input(tensor):
    """Check if the tensor is with a supported type."""
    if isinstance(tensor, np.ndarray):
        return
    if types.cupy_available():
        if isinstance(tensor, types.cp.ndarray):
            return
    if types.torch_available():
        if isinstance(tensor, types.th.Tensor):
            return
    raise RuntimeError(
        "Unrecognized tensor type '{}'. Supported types are: "
        "np.ndarray, torch.Tensor, cupy.ndarray.".format(type(tensor))
    )


def _check_rank_valid(g, rank: int):
    """Check the rank: 0 <= rank < world_size."""
    if rank < 0:
        raise ValueError("rank '{}' is negative.".format(rank))
    if rank >= g.world_size:
        raise ValueError(
            "rank '{}' must be less than world size '{}'".format(rank, g.world_size)
        )
