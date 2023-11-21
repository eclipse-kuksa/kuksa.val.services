# Continuous integration

## Naming of workflows

* for sub components like hvac_service and seat_service use pattern:
    **\<subcomponent>_\<action> eg: hvac_service**

* for overall tasks use pattern:
    **\<action> eg: release**

## Naming of Tags

* Release tags need to be in the following form:
    **\<major>.\<minor>.\<bugfix>\* eg: 1.2.1, 2.0.0**
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

See [Release Process](https://github.com/eclipse/kuksa.val.services/wiki/Release-Process)


