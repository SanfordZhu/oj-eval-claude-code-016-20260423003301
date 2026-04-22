#!/usr/bin/env python3
import random

# Generate test with many operations
n = 1000
print(n)

# Mix of operations
keys = [f"key{i}" for i in range(100)]
values = list(range(1000))

# Track expected state
state = {}

for _ in range(n):
    op = random.choice(['insert', 'find', 'delete'])
    key = random.choice(keys)

    if op == 'insert':
        value = random.choice(values)
        if key not in state:
            state[key] = set()
        state[key].add(value)
        print(f"insert {key} {value}")
    elif op == 'find':
        print(f"find {key}")
    else:  # delete
        if key in state and state[key]:
            value = random.choice(list(state[key]))
            state[key].remove(value)
            print(f"delete {key} {value}")
        else:
            # Try to delete non-existent
            value = random.choice(values)
            print(f"delete {key} {value}")

# Add some final finds to verify
for key in keys[:10]:
    print(f"find {key}")