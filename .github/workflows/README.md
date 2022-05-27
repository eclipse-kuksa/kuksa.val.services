# Continuous integration

## Naming of workflows

* for sub components like hvac_service and seat_service use pattern:
    **val_\<subcomponent>_\<action> eg: val_hvac_service**

* for overall tasks use pattern:
    **val_\<action> eg: val_release**

## Naming of Tags

* Release tags need to be in the following form:
    **v\<mayor>.\<minor>.\<bugfix>\* eg: v1.2.1, v2.0.0alpha**
## Workflow and branching model
![val_ci_workflow](./doc/val_ci_workflow.svg)

## How to create a new release
1. Adapt the version tags in all needed files, (e.g.: for v0.15.0) via executing
   * ``./prepare_release.sh 0.15.0``
2. tag a main branch version with a release tag
    * This trigger a github workflow which automatically creates a draft release
3. Publish the release on the repo webpage
   * navigate to the repo webpage -> Release -> edited the create draft -> Publish

