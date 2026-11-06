#!/usr/bin/env python3
"""Compatibility entrypoint for the unified original-owner channel codec test.

Engine25 replaced the four-volume-only layout and removed per-channel global
output reset. Keep callers of this historical test exercising the actual owner,
not a stale duplicate layout or reset assertion.
"""
import pathlib
import runpy
runpy.run_path(str(pathlib.Path(__file__).with_name('mixer-channel-history.py')), run_name='__main__')
