    Work in progress.
    
    TODO: 
      3030 Extrusion Stand Assembly
      Introduce "OSSM - Mod" project template

# Design Guidelines For Contributions

User-contributed Projects with a file structure following these guidelines will allow for the easiest incorporation into an official OSSM release.

![CAD Project Overview](Images/Overview.png)


## Fusion File Structure:
    ProjectName
        (Bodies)
          <empty>
        (Component)ComponentNameOne
          (Bodies)
            ...
        (Component)ComponentNameTwo
          (Bodies)
            ...
        (Component)Hardware - ProjectName 
          (Bodies)
            <empty>
          (Component)HardwareName:1
            (Bodies)
              ...
          (Component)HardwareName:2
            (Bodies)
              ...

## Naming Conventions

- **Disallowed/Special-Use Characters**: These characters are reserved for special behavior for documentation scripting and should not be part of design or component names.
  - `.` Designates file extension (dot) *[Disallowed]*
  - `-` Designates parent-child component relationship
    - Allowed for use in hardware names e.g. `M5 T-Nut`
  - `_` Designates a component (underscore) *[Disallowed]*
  - `[]` Designates reuse of hardware (brackets)
  - `()` Designates special hardware instructions (parentheses)

- **Single Component Project**: If your project will only have a single component for printing, it should contain only one component named identical to the ProjectName.

- **Parent-child Component Relationship**: If your part should be considered as a primary item for a parent component, indicate this by referencing the Parent Component by exact name, followed by ` - ` and then the name of your Child Component.
  - Example: `End Effector - 24mm Short` will cause the release file to be located in the 'End Effector' directory of files, named '24mm Short'.
  - At this time, only one Parent-child relationship is allowed per design file, and must be on a single level.

- **Version Increment**: If a piece of hardware is receiving an update from the initial release, append ` V#.#` (uppercase, case sensitive) to the component name, representative of Major.Minor updates.
  - Example: `Ring V1.1`

- **Reused Hardware**: If your part reuses hardware from a part it may be replacing, note this with ` [From:{Component}]` in the component name.
  - Example: `M3 x 8 Cap Head Bolt [From:OSSM - Body - Middle]`

- **Alternative Hardware**: If an alternative hardware can be used, note this with ` ({Specification} alternative)` in the component name.
  - Example: `M4x12 Cap Head Screw (M4 x 10 alternative)`

- **Variant Hardware**: If hardware should vary for a different type of hardware a user could have, note this with `({Specification} for {variant},{Specification2} for {variant2})` in the component name. 
  - Example: `(M6 x 15 Cap Head Screw for 3030 Extrusion,M6 x 20 Cap Head Screw for 4040 Extrusion)`

## Running a Release from Fusion

This section serves as documentation for OSSM Project Leads.

There is a proof of concept "Release" process that may evolve over time, but it relies on the above conventions being followed strictly.

### Future Work:
- Convert `.stp` to `.stl` at high-quality refinement and bundle into `OSSM-Hardware/Hardware/OSSM Printed Parts` release.
- Generate hardware documentation from `Hardware` named component structures.

### Steps:

1. Update the Fusion Document name to `OSSM Release $MM.DD.YYYY`.
2. Export your Fusion Document as `OSSM.f3z` to overwrite the current file.
3. Run the Add-In "ExportIt" using the settings below. This will populate the `_export` directory with `f3d` and `step` folders.
4. Run `OrganizeForRelease.ps1` in a PowerShell terminal from the directory containing the `_export` folder, next to `OSSM.f3z` 
  - This will create a `RELEASE` folder with the date appended 
  - Files will be organized in a release configuration
    - `.f3d` is grouped based on Fusion nested structure
    - `.stp` is grouped based on naming convention, children grouped into parent
    - [Future] Documentation framework generated referencing files contained in `Hardware` directories 
5. Manually tidy up:
   - Move the previous release to the `_archive` folder.
   - Check for loose files in `_export` and remove them.
     - (This may suggest something went wrong generating your release)

### ExportIt Settings

#### Export Tab
![ExportIt Export Tab Settings](Images/ExportIt_Export.png)

#### Location Tab
![ExportIt Location Tab Settings](Images/ExportIt_Location.png)

#### Misc Tab
![ExportIt Misc Tab Settings](Images/ExportIt_Misc.png)

