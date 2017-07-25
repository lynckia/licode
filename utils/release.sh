#!/bin/bash

usage() {
cat << EOF
usage: $0 [-p|-r] -v vX

Creates a pre/release
OPTIONS:
   -h      Show this message
   -p      Creates or Updates a new Prerelease with the given name
   -r      Creates a release with the given name
   -v      Version (vX)
EOF
}

while getopts “hpurv:” OPTION
do
  case $OPTION in
    h)
      usage
      exit
      ;;
    p)
      MODE="PRERELEASE"
      ;;
    r)
      MODE="RELEASE"
      ;;
    v)
      VERSION="$OPTARG"
      ;;
    ?)
      usage
      exit 1
      ;;
  esac
done

echo "INFO: Mode ($MODE) Version ($VERSION)"

if [ -z "$MODE" ] || [ -z "$VERSION" ]; then
  echo ERROR: Please provide -p or -r and -v
  usage
  exit
fi

git fetch

CURRENT_RELEASE_MAJOR=`echo "${VERSION}" | sed "s/v//g"`
PREVIOUS_VERSION=v`expr ${CURRENT_RELEASE_MAJOR} - 1`
CURRENT_PRERELEASE_MINOR=`git ls-remote --tags | grep pre-${VERSION} | tail -1 | sed "s/.*pre-${VERSION}\.//g"`
GITHUB_URL="https://api.github.com/repos/jcague/licode"
COMMIT=`git rev-list -n 1 HEAD`
SHORT_GIT_HASH=`echo ${COMMIT} | cut -c -7`

curl -s -H "Authorization: token ${GITHUB_OAUTH_TOKEN}" -X GET ${GITHUB_URL}/releases/tags/${VERSION} > /dev/null 2>&1
if [ $? -eq 1 ]; then
  echo WARNING: No previous version found
  PREVIOUS_VERSION=HEAD~10
fi

if [ -z "$CURRENT_PRERELEASE_MINOR" ]; then
  # Create a new PREPRELEASE
  NEXT_PRERELEASE_MINOR=1
else
  NEXT_PRERELEASE_MINOR=`expr ${CURRENT_PRERELEASE_MINOR} + 1`
fi

LATEST_PRERELEASE_NAME="pre-${VERSION}.${CURRENT_PRERELEASE_MINOR}"
NEXT_PRERELEASE_NAME="pre-${VERSION}.${NEXT_PRERELEASE_MINOR}"
RELEASE_NAME="${VERSION}"

if [ "$MODE" = "PRERELEASE" ]; then
  # Download docker
  docker login -u ${DOCKER_USER} -p ${DOCKER_PASS}
  docker pull lynckia/licode:${SHORT_GIT_HASH}

  # Tag with minor version and staging
  docker tag lynckia/licode:${SHORT_GIT_HASH} lynckia/licode:${NEXT_PRERELEASE_NAME}
  docker tag lynckia/licode:${SHORT_GIT_HASH} lynckia/licode:staging
  docker push lynckia/licode:${NEXT_PRERELEASE_NAME} lynckia/licode:staging
  echo Done.
elif [ "$MODE" = "RELEASE" ]; then
  if [ -z "$CURRENT_PRERELEASE_MINOR" ]; then
    echo ERROR: Prerelease does not exist
    usage
    exit 1
  fi

  # Download docker
  docker login -u ${DOCKER_USER} -p ${DOCKER_PASS}
  docker pull lynckia/licode:${LATEST_PRERELEASE_NAME}

  # Tag with minor version and staging
  docker tag lynckia/licode:${LATEST_PRERELEASE_NAME} lynckia/licode:${RELEASE_NAME}
  docker tag lynckia/licode:${LATEST_PRERELEASE_NAME} lynckia/licode:latest
  docker push lynckia/licode:${RELEASE_NAME} lynckia/licode:latest

  # TODO(javier): Remove lynckia/licode:XXX that belongs to this release

  # Create release in github
  COMMIT=`docker inspect lynckia/licode:${LATEST_PRERELEASE_NAME} | jq -r '.[0].Config.Labels.COMMIT'`
  LOGS=`git log $PREVIOUS_VERSION..$COMMIT --oneline | perl -p -e 's/\n/\\\\n/'`
  DESCRIPTION="### Detailed PR List:\\n$LOGS"

  # Create or update pre-release in github
  echo Creating Release...
  curl -s -H "Authorization: token ${GITHUB_OAUTH_TOKEN}" -H "Content-Type: application/json" -X POST -d "{\"tag_name\":\"${VERSION}\",\"name\":\"${VERSION}\",\"target_commitish\":\"${COMMIT}\",\"prerelease\":false,\"draft\":false,\"body\":\"${DESCRIPTION}\"}" ${GITHUB_URL}/releases
  echo Done.

  # TODO(javier): Update readthedocs
fi
