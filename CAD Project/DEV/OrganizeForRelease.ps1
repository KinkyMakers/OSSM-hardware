# Define the directories containing the files
$directories = @("_export/step/", "_export/f3d/")

# Get the current date in DD.MM.YYYY format
$currentDate = (Get-Date).ToString("MM.dd.yyyy")

# Define the base output directory
$baseOutputDirectory = "RELEASE_$currentDate"

# Remove the base output directory if it already exists
if (Test-Path -Path $baseOutputDirectory) {
    Remove-Item -Path $baseOutputDirectory -Recurse -Force
}

# Write the file OSSM.f3z to the output directory with release date, create directory if it doesn't exist
$ossmFile = "OSSM.f3z"
$ossmOutputDirectory = Join-Path -Path $baseOutputDirectory -ChildPath ""
$ossmOutputPath = Join-Path -Path $ossmOutputDirectory -ChildPath "OSSM_$currentDate.f3z"
if (-Not (Test-Path -Path $ossmOutputDirectory)) {
    New-Item -Path $ossmOutputDirectory -ItemType Directory -Force
}
Copy-Item -Path $ossmFile -Destination $ossmOutputPath -Force

# Process each directory
foreach ($directoryPath in $directories) {
    # Check if the directory exists
    if (-Not (Test-Path -Path $directoryPath)) {
        Write-Host "Directory not found: $directoryPath"
        continue
    }

    # Get all files in the directory, exclude files beginning with a dot (.gitignore)
    $files = Get-ChildItem -Path $directoryPath -Exclude ".*"

    # Process each file
    foreach ($file in $files) {
        # Get the base name of the file (without extension)
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($file.Name)
        
        # Get the file extension
        $extension = $file.Extension
        
        # Remove everything after and including the final '_'
        $cleanedName = $baseName -replace '_[^_]*$', ''
        
        # Remove spaces directly before and/or after '-' and '_'
        $cleanedName = $cleanedName -replace '\s*-\s*', '-' -replace '\s*_\s*', '_'
        
        # Split out the last component name (everything after the last '_')
        $lastComponentName = $cleanedName -replace '.*_', ''
        
        # Split the cleaned name into components
        $components = $cleanedName -split '_'
        
        # Initialize case detection variables
        $containsHardware = $false
        $parentChildRelationship = $false
        $parentComponent = ""
        $childComponent = ""
        
        # Detect Case 1: The string 'Hardware-' is found
        if ($cleanedName -like '*Hardware-*') {
            $containsHardware = $true
        }
        
        # Detect Case 2: The last component name is found somewhere other than the last component in the component list
        # Only apply these smarts if the directory path is "_export/step/"
        if ($components[0..($components.Length - 2)] -contains $lastComponentName) {
            # Just make the changes here, then we don't need special considerations when creating the output path
            # Remove everything after the first occurrence of the last component name
            $components = $components[0..($components.IndexOf($lastComponentName))]
        }
        
        # Detect Case 3: The last component name has a '-' in it (parent-child relationship)
        if ($lastComponentName -like '*-*') {
            $parentChildRelationship = $true
            $parentComponent, $childComponent = $lastComponentName -split '-', 2
        }
        
        # Split the components on '-'
        $components = $components -join '-' -split '-'
        
        # Generate the output path
        $outputDirectory = $baseOutputDirectory
        
        if ($containsHardware) {
            #Find the first occurrance of 'hardware' in components
            $hardwareIndex = $components.IndexOf('Hardware')
            if ($hardwareIndex -ge 0) {
                $outputDirectory = Join-Path -Path $outputDirectory -ChildPath (Join-Path -Path ($components[0..$hardwareIndex] -join '\') -ChildPath "")
            }
            $outputFilename = $lastComponentName + $extension

        }
        elseif ($parentChildRelationship -and $directoryPath -like "*_export/step/*") {
            $outputDirectory = Join-Path -Path $outputDirectory -ChildPath (Join-Path -Path ($components[0..($components.Length - 2)] -join '\') -ChildPath "")
            $outputFilename = ($components[0..($components.Length - 2)] -join ' - ') + " - " + $childComponent + $extension
        }
        elseif ($parentChildRelationship -and $directoryPath -like "*_export/f3d/*") {
            $outputDirectory = Join-Path -Path $outputDirectory -ChildPath (Join-Path -Path ($components[0..($components.Length - 2)] -join '\') -ChildPath $childComponent)
            $outputFilename = ($components[0..($components.Length - 2)] -join ' - ') + " - " + $childComponent + $extension
        }
        else {
            if ($components.Length -gt 1) {
                $outputDirectory = Join-Path -Path $outputDirectory -ChildPath (Join-Path -Path ($components[0..($components.Length - 2)] -join '\') -ChildPath "")
            }
            $outputFilename = ($components[0..($components.Length - 2)] -join ' - ') + " - " + $lastComponentName + $extension
        }
        $outputPath = Join-Path -Path $outputDirectory -ChildPath $outputFilename
        
        # Create the output directory if it doesn't exist
        if (-Not (Test-Path -Path $outputDirectory)) {
            New-Item -Path $outputDirectory -ItemType Directory -Force
        }
        
        # Copy the file to the output path
        Copy-Item -Path $file.FullName -Destination $outputPath -Force

        # Remove original file if the copy was successful
        if (Test-Path -Path $outputPath) {
            Remove-Item -Path $file.FullName -Force
        }
        else {
            Write-Host "Failed to copy file: $($file.FullName)"
        }

    }
}