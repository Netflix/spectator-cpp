#!/usr/bin/env bash

MAYBE_PYTHON=$(find /apps -maxdepth 1 -type l -name "python*")

if [[ -n "$MAYBE_PYTHON" ]]; then
  PYTHON3="$MAYBE_PYTHON/bin/python3"
  echo "using $PYTHON3 ($($PYTHON3 -V))"
else
  PYTHON3="python3"
  echo "using $(which $PYTHON3) ($($PYTHON3 -V))"
fi

# create and activate virtualenv
$PYTHON3 -m venv venv
source venv/bin/activate

if [[ -f requirements.txt ]]; then
    # use the virtualenv python
    python -m pip install --upgrade pip wheel
    python -m pip install --requirement requirements.txt
fi