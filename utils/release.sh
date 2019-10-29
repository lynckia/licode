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

mkdir -p ~/.ssh
ssh-keyscan -H github.com >> ~/.ssh/known_hosts

COMMIT=`git rev-list -n 1 HEAD`
git fetch
git checkout $COMMIT

CURRENT_RELEASE_MAJOR=`echo "${VERSION}" | sed "s/v//g"`
PREVIOUS_VERSION=v`expr ${CURRENT_RELEASE_MAJOR} - 1`
CURRENT_PRERELEASE_MINOR=`git ls-remote --tags | grep pre-${VERSION} | sed "s/.*pre-${VERSION}\.//g" | sort -n | tail -1`
GITHUB_URL="https://api.github.com/repos/lynckia/licode"

SHORT_GIT_HASH=`echo ${COMMIT} | cut -c -7`

curl -s -u ${GITHUB_OAUTH_USER}:${GITHUB_OAUTH_TOKEN} -X GET ${GITHUB_URL}/releases/tags/${VERSION} > /dev/null 2>&1
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

URL=`curl -s -u ${GITHUB_OAUTH_USER}:${GITHUB_OAUTH_TOKEN} -X GET ${GITHUB_URL}/releases/tags/${LATEST_PRERELEASE_NAME} | jq -r '.url'`

if [ "$MODE" = "PRERELEASE" ]; then
  # Download docker
  docker login -u ${DOCKER_USER} -p ${DOCKER_PASS}
  docker pull lynckia/licode:${SHORT_GIT_HASH}

  # Tag with minor version and staging
  docker tag lynckia/licode:${SHORT_GIT_HASH} lynckia/licode:${NEXT_PRERELEASE_NAME}
  docker tag lynckia/licode:${SHORT_GIT_HASH} lynckia/licode:staging
  docker tag lynckia/licode:${SHORT_GIT_HASH} lynckia/licode:latest
  docker push lynckia/licode:${NEXT_PRERELEASE_NAME}
  docker push lynckia/licode:staging
  docker push lynckia/licode:latest

  LOGS=`git log $PREVIOUS_VERSION..$COMMIT --oneline | perl -p -e 's/\n/\\\\n/' | sed -e s/\"//g`
  DESCRIPTION="### Detailed PR List:\\n$LOGS"

  if [ "$URL" = "null" ]; then
    echo Create new Prerelease...
    curl -s -u ${GITHUB_OAUTH_USER}:${GITHUB_OAUTH_TOKEN} -H "Content-Type: application/json" -X POST -d "{\"tag_name\":\"${NEXT_PRERELEASE_NAME}\",\"name\":\"${VERSION}\",\"target_commitish\":\"${COMMIT}\",\"prerelease\":true,\"draft\":false,\"body\":\"${DESCRIPTION}\"}" ${GITHUB_URL}/releases
  else
    echo Update Prerelease...
    curl -s -u ${GITHUB_OAUTH_USER}:${GITHUB_OAUTH_TOKEN} -H "Content-Type: application/json" -X PATCH -d "{\"tag_name\":\"${NEXT_PRERELEASE_NAME}\",\"target_commitish\":\"${COMMIT}\",\"body\":\"${DESCRIPTION}\"}" ${URL}
  fi

  echo Done.
elif [ "$MODE" = "RELEASE" ]; then
  if [ -z "$CURRENT_PRERELEASE_MINOR" ] || [ "$URL" = "null" ]; then
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
  docker push lynckia/licode:${RELEASE_NAME}
  docker push lynckia/licode:latest

  # TODO(javier): Remove lynckia/licode:XXX that belongs to this release

  # Create release in github
  echo Creating Release...
  curl -s -u ${GITHUB_OAUTH_USER}:${GITHUB_OAUTH_TOKEN} -H "Content-Type: application/json" -X PATCH -d "{\"tag_name\":\"${RELEASE_NAME}\",\"prerelease\":false}" ${URL}
  echo Done.

  # TODO(javier): Update readthedocs
fi
