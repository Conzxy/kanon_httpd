#!/bin/bash
awk -f connected.awk kanon/*> connected && awk -f disconn.awk kanon/*> disconnected
diff connected disconnected
rm connected disconnected
