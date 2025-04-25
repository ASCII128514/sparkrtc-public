#!/usr/bin/env python3
# Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""
Simple script to check if a directory exists.
Used by GN build files to conditionally enable features based on available dependencies.
"""

import os
import sys

def main():
    if len(sys.argv) != 2:
        print("Usage: {} <directory_path>".format(sys.argv[0]))
        return 1
    
    # Convert GN-style paths (//foo/bar) to regular paths
    path = sys.argv[1]
    if path.startswith('//'):
        # In GN, // refers to the root of the source tree
        # We need to convert this to a path relative to the current directory
        path = path[2:]  # Remove the leading //
        
        # Find the root of the source tree (where the .gn file is)
        root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        path = os.path.join(root_dir, path)
    
    # Check if the directory exists
    exists = os.path.isdir(path)
    print(str(exists))
    return 0

if __name__ == '__main__':
    sys.exit(main())