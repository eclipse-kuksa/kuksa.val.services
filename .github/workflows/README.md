# Continuous integration

## Naming of workflows

* for sub components like hvac_service and seat_service use pattern:
    **\<subcomponent>_\<action> eg: hvac_service**

* for overall tasks use pattern:
    **\<action> eg: release**

## Naming of Tags

* Release tags need to be in the following form:
    **v\<mayor>.\<minor>.\<bugfix>\* eg: v1.2.1, v2.0.0alpha**
## Workflow and branching model
![ci_workflow](./doc/ci_workflow.svg)


## Naming of artifacts

Artifacts result from the workflow are used in other workflows and might be published directly to a release page, therefore their naming matters.
In general the naming shall follow: **<type>(_<sub-type>)_<componenent-name>.tar/zip**

* Application binaries: **bin_\<component-name\>_<hwarch>_<buildtype>.tar**, e.g. bin_vservice-seat_x86_64_release.tar.gz
* Containers layers: **oci_\<component-name\>.tar**, e.g. oci_vservice-seat.tar
* Test result reports: **report_test_\<component-name\>**, e.g. report_test_vservice-seat-ctl
* Test coverage reports: **report_codecov_\<component-name\>.\**, e.g. report_codecov_vservice-seat-ctl
* Documentation: **docu_\<component-name\>.\**, e.g: docu_vservice-seat

## How to create a new release
1. Adapt the version tags in all needed files, (e.g.: for v0.15.0) via executing
   * ``./prepare_release.sh 0.15.0``
2. tag a main branch version with a release tag
    * This trigger a github workflow which automatically creates a draft release
3. Publish the release on the repo webpage
   * navigate to the repo webpage -> Release -> edited the create draft -> Publish

