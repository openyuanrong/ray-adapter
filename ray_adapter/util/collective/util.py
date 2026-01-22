"""Some utility class for Collectives."""
import asyncio
import logging

import ray_adapter as ray

logger = logging.getLogger(__name__)



@ray.remote
class Info:
    """Store the group information created via `create_collective_group`.

    Note: Should be used as a NamedActor.
    """

    def __init__(self):
        self.ids = None
        self.world_size = -1
        self.rank = -1
        self.backend = None
        self.gloo_timeout = 30000

    def set_info(self, ids, world_size, rank, backend, gloo_timeout):
        """Store collective information."""
        self.ids = ids
        self.world_size = world_size
        self.rank = rank
        self.backend = backend
        self.gloo_timeout = gloo_timeout

    def get_info(self):
        """Get previously stored collective information."""
        return (
            self.ids,
            self.world_size,
            self.rank,
            self.backend,
            self.gloo_timeout,
        )
